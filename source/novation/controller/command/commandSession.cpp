#include "commandSession.h"

#include <cassert>
#include <limits>

namespace novation::controller::command
{
    Session CommandSession::coldReset(Model model, HostFrame host, NativeFrame native, ClockRate hostRate,
                                      ClockRate nativeRate, uint32_t lead) noexcept
    {
        const auto map = HostMap::make(host, native, hostRate, nativeRate);
        if (dispatching_ || !map || (model != Model::K && model != Model::KS))
            return {};
        const auto owner = scheduler_.reset(native);
        if (!owner.owner)
            return {};
        const bool reset = transactions_.coldReset(owner, model);
        assert(reset);
        if (!reset)
        {
            scheduler_.stop(owner);
            return {};
        }
        decoder_.reset();
        session_ = owner;
        model_ = model;
        hostMap_ = map;
        nativeRate_ = nativeRate;
        lead_ = lead;
        rateGeneration_ = 1;
        return owner;
    }

    bool CommandSession::stop(Session owner) noexcept
    {
        if (!scheduler_.stop(owner))
            return false;
        transactions_.stop(owner);
        decoder_.reset();
        return true;
    }

    AdmissionStatus CommandSession::lifecycle(Session owner) const noexcept
    {
        if (owner != session_ || !owner.owner)
            return AdmissionStatus::StaleSession;
        return scheduler_.running(owner) ? AdmissionStatus::Accepted : AdmissionStatus::Stopped;
    }

    bool CommandSession::validPart(uint8_t part) const noexcept
    {
        return part == RoutedByChannel || part < (model_ == Model::K ? 1 : 4);
    }

    bool CommandSession::validEndpoint(Endpoint endpoint) const noexcept
    {
        return endpoint == Endpoint::A || (model_ == Model::KS && endpoint == Endpoint::B);
    }

    bool CommandSession::rebaseHost(Session owner, HostFrame at, ClockRate rate, uint32_t lead) noexcept
    {
        if (dispatching_ || lifecycle(owner) != AdmissionStatus::Accepted ||
            rateGeneration_ == std::numeric_limits<uint64_t>::max())
            return false;
        const auto map = hostMap_->rebase(at, rate, nativeRate_);
        if (!map)
            return false;
        hostMap_ = map;
        lead_ = lead;
        ++rateGeneration_;
        return true;
    }

    std::optional<HostBlock> CommandSession::hostBlock(Session owner, HostFrame first, uint32_t frames) const noexcept
    {
        if (lifecycle(owner) != AdmissionStatus::Accepted || !frames ||
            frames - 1 > std::numeric_limits<uint64_t>::max() - first.ticks || !hostMap_->map(first))
            return std::nullopt;
        return HostBlock{owner, rateGeneration_, first, frames};
    }

    std::optional<NativeFrame> CommandSession::hostDeadline(HostBlock block, uint32_t offset) const noexcept
    {
        if (lifecycle(block.session) != AdmissionStatus::Accepted || block.rateGeneration != rateGeneration_ ||
            offset >= block.frames || offset > std::numeric_limits<uint64_t>::max() - block.first.ticks)
            return std::nullopt;
        const auto native = hostMap_->map({block.first.ticks + offset});
        if (!native || lead_ > std::numeric_limits<uint64_t>::max() - native->ticks)
            return std::nullopt;
        return NativeFrame{native->ticks + lead_};
    }

    Admission CommandSession::submit(Session owner, NativeFrame due, ControllerEvent event) noexcept
    {
        const auto state = lifecycle(owner);
        if (state != AdmissionStatus::Accepted)
            return {state};
        if (!validPart(event.part) || std::holds_alternative<DspReply>(event.payload))
            return {AdmissionStatus::InvalidEvent};
        return scheduler_.submit(owner, due, event);
    }

    MidiAdmission CommandSession::pushMidiByte(Session owner, NativeFrame due, uint8_t byte) noexcept
    {
        Admission admission{lifecycle(owner)};
        if (admission.status != AdmissionStatus::Accepted || dispatching_)
        {
            if (dispatching_ && admission.status == AdmissionStatus::Accepted)
                admission.status = AdmissionStatus::InvalidEvent;
            return {admission, MidiDecoder::Result::Backpressure, false};
        }
        const auto decode = decoder_.push(byte, [&](const EventPayload& message)
        {
            admission = submit(owner, due, {message});
            return admission.status == AdmissionStatus::Accepted;
        });
        return {admission, decode, decode != MidiDecoder::Result::Backpressure};
    }

    MidiAdmission CommandSession::pushHostMidiByte(HostBlock block, uint32_t offset, uint8_t byte) noexcept
    {
        const auto state = lifecycle(block.session);
        if (state != AdmissionStatus::Accepted)
            return {{state}, MidiDecoder::Result::Backpressure, false};
        const auto due = hostDeadline(block, offset);
        return due ? pushMidiByte(block.session, *due, byte) :
            MidiAdmission{{AdmissionStatus::InvalidEvent}, MidiDecoder::Result::Backpressure, false};
    }

    AdmissionStatus CommandSession::beginPrepared(Session owner, PreparedCursor& cursor, const PreparedMidi& batch,
                                                  NativeFrame due, uint8_t part) const noexcept
    {
        const auto state = lifecycle(owner);
        if (state != AdmissionStatus::Accepted)
            return state;
        if (cursor.batch_ || !batch.valid() || !validPart(part))
            return AdmissionStatus::InvalidEvent;
        cursor.batch_ = &batch;
        cursor.session_ = owner;
        cursor.due_ = due;
        cursor.part_ = part;
        cursor.next_ = 0;
        return AdmissionStatus::Accepted;
    }

    PreparedDrain CommandSession::drainPrepared(PreparedCursor& cursor, size_t budget) noexcept
    {
        if (!cursor.batch_ || dispatching_)
            return {AdmissionStatus::InvalidEvent, 0, false};
        const auto state = lifecycle(cursor.session_);
        if (state != AdmissionStatus::Accepted)
            return {state, 0, false};
        size_t count = 0;
        while (count < budget && cursor.next_ < cursor.batch_->eventCount())
        {
            const auto event = cursor.batch_->eventAt(cursor.next_);
            if (!event)
                return {AdmissionStatus::InvalidEvent, count, false};
            const auto admission = submit(cursor.session_, cursor.due_, {*event, cursor.part_});
            if (admission.status != AdmissionStatus::Accepted)
                return {admission.status, count, false};
            ++cursor.next_;
            ++count;
        }
        const bool complete = cursor.next_ == cursor.batch_->eventCount();
        if (complete)
            releasePrepared(cursor);
        return {AdmissionStatus::Accepted, count, complete};
    }

    bool CommandSession::releasePrepared(PreparedCursor& cursor) const noexcept
    {
        if (!cursor.batch_ || cursor.session_.owner != session_.owner)
            return false;
        cursor.batch_ = nullptr;
        return true;
    }

    bool CommandSession::feedbackRoom(Session owner, Endpoint endpoint) const noexcept
    {
        return !dispatching_ && validEndpoint(endpoint) && scheduler_.feedbackRoom(owner, endpoint);
    }

    Admission CommandSession::receiveReply(DspTicket ticket, NativeFrame observedAt, uint32_t word) noexcept
    {
        const auto state = lifecycle(ticket.session);
        if (state != AdmissionStatus::Accepted)
            return {state};
        DspTransactionProgress progress;
        if (dispatching_ || !validEndpoint(ticket.endpoint) || word > 0xffffff ||
            !transactions_.progress(ticket, progress) || progress.status != DspStatus::Transferred ||
            progress.releaseEndpoint != DspMilestone::Observation || progress.address.mode == DspAddressMode::Write)
            return {AdmissionStatus::InvalidEvent};
        const auto admission = scheduler_.submitFeedback(ticket.session, observedAt, {ticket.endpoint, ticket.sequence, word});
        if (admission.status == AdmissionStatus::Accepted)
        {
            const bool recorded = transactions_.recordObserved(ticket);
            assert(recorded);
            (void)recorded;
        }
        return admission;
    }

    bool CommandSession::notifyDsp(WakeToken wake, NativeFrame readyAt) noexcept
    {
        return validEndpoint(wake.cause.endpoint) && scheduler_.notifyDsp(wake, readyAt);
    }

    PumpResult CommandSession::runUntil(NativeFrame horizon, unsigned budget, MusicalDispatch dispatch) noexcept
    {
        if (dispatching_)
            return {PumpStatus::InvalidHorizon, 0, 0};
        struct Call
        {
            CommandSession& owner;
            MusicalDispatch dispatch;
        } call{*this, dispatch};
        const auto result = scheduler_.runUntil(horizon, budget, {&call,
            [](void* opaque, const ScheduledEvent& event, NativeFrame now) noexcept -> DispatchDecision
            {
                auto& call = *static_cast<Call*>(opaque);
                auto& owner = call.owner;
                owner.dispatching_ = true;
                const auto decision = call.dispatch.handle ?
                    call.dispatch.handle(call.dispatch.context, owner, event, now) :
                    DispatchDecision{DispatchDecision::Status::Fault, {}};
                owner.dispatching_ = false;
                if (decision.status == DispatchDecision::Status::WaitForDsp && !owner.validEndpoint(decision.wait.endpoint))
                    return {DispatchDecision::Status::Fault, {}};
                return decision;
            }});
        if (result.status == PumpStatus::Fault)
        {
            transactions_.stop(session_);
            decoder_.reset();
        }
        return result;
    }
}
