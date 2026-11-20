#include "kCommandVoiceFeedback.h"
#include <exception>

namespace novation::controller::command
{
    KCommandVoiceFeedback::~KCommandVoiceFeedback()
    { if (accepted_ && !complete_ && !fault_) fail(); }

    bool KCommandVoiceFeedback::prepare(CommandSession& session, KMusicalState& state, VoiceAllocation allocation) noexcept
    {
        if (state_ || fault_ || session.model()!=Model::K || session.snapshot().session != state.session() ||
            session.snapshot().lifecycle != Lifecycle::Running || !state.voiceOperationAvailable()) return false;
        const auto voice = state.voice(allocation);
        const auto source = state.feedback(allocation);
        const auto dependency = state.dependency(session.rateGeneration());
        if (!voice || !source || !source->age || !dependency || state.activationPending(allocation) ||
            voice->observationRevision == std::numeric_limits<uint64_t>::max()) return false;
        session_ = &session; state_ = &state; allocation_ = allocation;
        before_ = *source; dependency_ = *dependency;
        observation_ = voice->observationRevision; operationRevision_ = state.voiceOperationRevision();
        return true;
    }

    bool KCommandVoiceFeedback::live() const noexcept
    {
        return state_ && !fault_ && session_->snapshot().session == state_->session() &&
            session_->snapshot().lifecycle == Lifecycle::Running && session_->rateGeneration() == dependency_.rateGeneration;
    }
    bool KCommandVoiceFeedback::current() const noexcept
    {
        const auto source = state_->feedback(allocation_);
        const auto voice = state_->voice(allocation_);
        const auto dependency = state_->dependency(dependency_.rateGeneration);
        return source && *source == before_ && voice && voice->observationRevision == observation_ &&
            dependency && *dependency == dependency_ &&
            state_->voiceOperationRevision() == operationRevision_ + unsigned(accepted_) &&
            (accepted_ ? state_->ownsVoiceOperation(this) : state_->voiceOperationAvailable());
    }
    DspAdmission KCommandVoiceFeedback::admit(uint64_t operation, NativeFrame at) noexcept
    {
        if (!live()) return reject(DspAdmission::StaleSession);
        if (ticket_ || complete_) return DspAdmission::StaleOperation;
        if (!current()) return reject(DspAdmission::InvalidDraft);
        const auto draft = KFeedbackProtocol::voiceRead(allocation_.slot, before_.age, at);
        if (!draft) return reject(DspAdmission::InvalidDraft);
        DspTicket ticket;
        const auto status = session_->transactions().submit(state_->session(), operation, &*draft, 1, &ticket, [&]() noexcept {
            if (!state_->claimVoiceOperation(this)) std::terminate();
            accepted_ = true;
        });
        if (status == DspAdmission::Accepted) ticket_ = ticket;
        else if (status != DspAdmission::Full) return reject(status);
        return status;
    }
    KFeedbackProgress KCommandVoiceFeedback::accept(DspReply reply, NativeFrame at) noexcept
    {
        if (!live() || !current() || !ticket_ || reply.endpoint != Endpoint::A ||
            reply.transaction != ticket_->sequence || reply.word > 0xffffff ||
            session_->transactions().poisoned(Endpoint::A) || !state_->canPublish(at)) return fail();
        const auto draft = KFeedbackProtocol::voiceRead(allocation_.slot, before_.age, at);
        DspTransactionProgress progress;
        if (!draft || !session_->transactions().progress(*ticket_, progress) || progress.status != DspStatus::Observed ||
            progress.address.mode != DspAddressMode::Read || progress.address.address != draft->address.address) return fail();
        const auto value = KFeedbackProtocol::result(before_, reply.word);
        if (!value || !state_->commitVoiceFeedback(allocation_, observation_, *value, at)) return fail();
        if (!session_->transactions().retire(*ticket_) || !state_->releaseVoiceOperation(this)) std::terminate();
        ticket_.reset(); result_ = *value; complete_ = true;
        return KFeedbackProgress::Complete;
    }
    KFeedbackProgress KCommandVoiceFeedback::progress() const noexcept
    {
        if (!live()) return KFeedbackProgress::Fault;
        return complete_ ? KFeedbackProgress::Complete : ticket_ ? KFeedbackProgress::WaitingForReply : KFeedbackProgress::Ready;
    }
    KFeedbackProgress KCommandVoiceFeedback::fail() noexcept
    {
        if (state_ && accepted_ && !complete_ && session_->snapshot().session == state_->session()) session_->stop(state_->session());
        if (state_ && state_->ownsVoiceOperation(this)) state_->releaseVoiceOperation(this);
        fault_ = true; return KFeedbackProgress::Fault;
    }
    DspAdmission KCommandVoiceFeedback::reject(DspAdmission status) noexcept
    { if (accepted_) fail(); return status; }
    bool KCommandVoiceFeedback::reset() noexcept
    {
        if (accepted_ && !complete_ && !fault_) return false;
        state_ = nullptr; session_ = nullptr; ticket_.reset();
        accepted_ = complete_ = fault_ = false; return true;
    }
}
