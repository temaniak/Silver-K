#pragma once

#include "ksCommandNoteRelease.h"

namespace novation::controller::command
{
    enum class KSAllSoundProgress { Ready, WaitingForTransfer, Complete, Fault };

    // KS CC120 is a global physical-voice cut, independent of part/key/pedal
    // state. One immutable intent owns all fourteen cut/gate pairs. Each pair
    // reserves atomically; the next pair waits for actual transfer progress.
    class KSCommandAllSoundOff final
    {
    public:
        KSCommandAllSoundOff()=default;
        ~KSCommandAllSoundOff(){if(accepted_ && !complete_ && !fault_)fail();}
        KSCommandAllSoundOff(const KSCommandAllSoundOff&)=delete;
        KSCommandAllSoundOff& operator=(const KSCommandAllSoundOff&)=delete;

        bool prepare(CommandSession& session,KSMusicalState& state) noexcept
        {
            if(state_ || session.snapshot().session!=state.session() ||
               session.snapshot().lifecycle!=Lifecycle::Running || !state.voiceOperationAvailable())return false;
            state_=&state;session_=&session;rate_=session.rateGeneration();revision_=state.voiceOperationRevision();
            for(size_t p=0;p<parts_.size();++p) {
                const auto part=KSStorageSlot(p);parts_[p]=state.dependency(part,rate_);
                if(parts_[p]) {keyRevisions_[p]=state.keys(part)->revision();sustainRevisions_[p]=state.sustain(part)->revision;}
            }
            for(size_t i=0;i<before_.size();++i) {
                before_[i]=state.voice(VoiceSlot(i));generations_[i]=*state.voiceGeneration(VoiceSlot(i));
                pending_[i]=state.deferredVoice(VoiceSlot(i));
                if(before_[i] && before_[i]->feedbackAge) {
                    if(before_[i]->observationRevision==std::numeric_limits<uint64_t>::max()){state_=nullptr;return false;}
                    slots_[count_++]=VoiceSlot(i);
                }
            }
            return true;
        }

        DspAdmission admit(uint64_t operation,NativeFrame at) noexcept
        {
            if(!live())return rejected(DspAdmission::StaleSession);
            if(complete_ || ticket_)return DspAdmission::StaleOperation;
            if(!current() || !state_->canPublish(at))return rejected(DspAdmission::InvalidDraft);
            if(session_->transactions().poisoned(Endpoint::B))return rejected(DspAdmission::Poisoned);
            if(!count_) {
                if(!state_->claimVoiceOperation(this) || !state_->releaseVoiceOperation(this))std::terminate();
                accepted_=complete_=true;return DspAdmission::Accepted;
            }
            const auto slot=size_t(slots_[next_]);const uint32_t cut=0x100000,gate=0;
            std::array<DspTransactionDraft,2> drafts{};
            for(auto& draft:drafts){draft.model=Model::KS;draft.endpoint=Endpoint::B;draft.earliest=at;draft.wordCount=1;}
            drafts[0].address={DspAddressMode::Write,uint16_t(0x72+101*slot)};drafts[0].words=&cut;
            drafts[1].address={DspAddressMode::Write,uint16_t(0x8085+22*slot)};drafts[1].words=&gate;
            const auto result=session_->transactions().submit(state_->session(),operation,drafts.data(),2,tickets_.data(),[&]()noexcept{
                if(!accepted_) {
                    if(!state_->claimVoiceOperation(this))std::terminate();
                    // Commit the complete musical intent once; the owner now
                    // retains all remaining physical work until success/fault.
                    for(size_t i=0;i<count_;++i) {
                        const auto& voice=*before_[size_t(slots_[i])];
                        if(!state_->observeVoiceFeedback(voice.allocation,uint16_t((voice.feedbackAge&0x3fff)|1)))std::terminate();
                    }
                    if(!state_->publish(at))std::terminate();accepted_=true;
                }
                ticket_=true;
            });
            return result==DspAdmission::Full || result==DspAdmission::Accepted?result:rejected(result);
        }

        KSAllSoundProgress progress() noexcept
        {
            if(complete_ && live())return KSAllSoundProgress::Complete;
            if(!live() || !accepted_ || !current() || session_->transactions().poisoned(Endpoint::B))return fail();
            if(!ticket_)return KSAllSoundProgress::Ready;
            for(const auto ticket:tickets_) {
                DspTransactionProgress progress;
                if(!session_->transactions().progress(ticket,progress))return fail();
                if(progress.status==DspStatus::Queued)return KSAllSoundProgress::WaitingForTransfer;
                if(progress.status!=DspStatus::Transferred)return fail();
            }
            for(const auto ticket:tickets_)if(!session_->transactions().retire(ticket))return fail();
            ticket_=false;
            if(++next_!=count_)return KSAllSoundProgress::Ready;
            if(!state_->releaseVoiceOperation(this))return fail();
            complete_=true;return KSAllSoundProgress::Complete;
        }

        bool reset() noexcept
        {
            if(accepted_ && !complete_ && !fault_)return false;
            state_=nullptr;session_=nullptr;count_=next_=0;ticket_=accepted_=complete_=fault_=false;return true;
        }
        KSAllSoundProgress abort() noexcept{return fail();}
        size_t size()const noexcept{return count_;}
        VoiceSlot slot()const noexcept{return next_<count_?slots_[next_]:VoiceSlot(0);}
        std::optional<DspTicket> ticket(size_t i)const noexcept
        {return ticket_ && i<2?std::optional{tickets_[i]}:std::nullopt;}

    private:
        bool live()const noexcept
        {return state_ && !fault_ && session_->snapshot().session==state_->session() &&
            session_->snapshot().lifecycle==Lifecycle::Running && session_->rateGeneration()==rate_;}
        bool current()const noexcept
        {
            if(state_->voiceOperationRevision()!=revision_+(accepted_?1:0) ||
               (accepted_?!state_->ownsVoiceOperation(this):!state_->voiceOperationAvailable()))return false;
            for(size_t p=0;p<parts_.size();++p) {
                const auto part=KSStorageSlot(p);const auto now=state_->dependency(part,rate_);
                if(bool(now)!=bool(parts_[p]) || (now && (!(*now==*parts_[p]) ||
                   state_->keys(part)->revision()!=keyRevisions_[p] || state_->sustain(part)->revision!=sustainRevisions_[p])))return false;
            }
            for(size_t i=0;i<before_.size();++i) {
                const auto now=state_->voice(VoiceSlot(i));const auto& before=before_[i];
                const auto delta=accepted_ && before && before->feedbackAge?1u:0u;
                if(*state_->voiceGeneration(VoiceSlot(i))!=generations_[i] ||
                   !(pending_[i]==state_->deferredVoice(VoiceSlot(i))) || bool(now)!=bool(before) ||
                   (now && (!(now->allocation==before->allocation) || now->observationRevision!=before->observationRevision+delta ||
                    now->feedbackAge!=(delta?uint16_t((before->feedbackAge&0x3fff)|1):before->feedbackAge))))return false;
            }
            return true;
        }
        DspAdmission rejected(DspAdmission reason)noexcept{if(accepted_)fail();return reason;}
        KSAllSoundProgress fail()noexcept
        {
            if(state_ && accepted_ && session_->snapshot().session==state_->session())session_->stop(state_->session());
            if(state_ && state_->ownsVoiceOperation(this))state_->releaseVoiceOperation(this);
            fault_=true;return KSAllSoundProgress::Fault;
        }
        KSMusicalState* state_=nullptr;CommandSession* session_=nullptr;
        uint64_t rate_=0,revision_=0;
        std::array<std::optional<ParameterDependency<KSStorageSlot>>,5> parts_{};
        std::array<uint64_t,5> keyRevisions_{},sustainRevisions_{};
        KSMusicalState::Voices::Projection before_{};
        KSMusicalState::DeferredVoices pending_{};
        std::array<uint64_t,14> generations_{};
        std::array<VoiceSlot,14> slots_{};
        std::array<DspTicket,2> tickets_{};
        size_t count_=0,next_=0;
        bool ticket_=false,accepted_=false,complete_=false,fault_=false;
    };
}
