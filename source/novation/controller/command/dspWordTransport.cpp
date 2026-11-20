#include "dspWordTransport.h"

#include <algorithm>
#include <limits>

namespace novation::controller::command
{
    namespace
    {
        constexpr auto LastTick = std::numeric_limits<uint64_t>::max();
        constexpr uint8_t TxEmpty = 0x02;
        constexpr uint8_t RxFull = 0x01;
        constexpr uint8_t TxReady = 0x04;
        constexpr uint8_t DspHf3 = 0x10;
        constexpr uint8_t HostHf0 = 0x08;
    }

    bool DspWordTransport::begin(DspTicket ticket, BusCycle first, HdiWordTiming timing) noexcept
    {
        if (action_ != Action::Idle || ticket.endpoint != endpoint_ || !timing.strobeSpacing ||
            !timing.pollInterval || !timing.readyTimeout || timing.readyTimeout > LastTick - first.ticks ||
            (ticket_.session == ticket.session && lastAccess_ && !(*lastAccess_ < first)))
            return false;
        if (ticket_.session != ticket.session)
            lastAccess_.reset();
        ticket_ = ticket;
        timing_ = timing;
        availability_ = {};
        next_ = first;
        first_ = first;
        expires_ = {first.ticks + timing.readyTimeout};
        blockedSince_.reset();
        addressAccepted_ = false;
        addressMode_ = DspAddressMode::None;
        retryLow_ = false;
        action_ = Action::SampleStatus;
        return true;
    }

    bool DspWordTransport::beginOffered(CommandSession& session, const DspOffer& offer, NativeFrame now,
                                      BusCycle first, HdiWordTiming timing) noexcept
    {
        if (offer.status != DspOfferStatus::Ready)
            return false;
        const auto current = session.transactions().offer(offer.ticket.session, endpoint_, now);
        if (current.status != DspOfferStatus::Ready || current.ticket.sequence != offer.ticket.sequence ||
            current.ticket.slot != offer.ticket.slot || current.offset != offer.offset || current.word != offer.word ||
            !begin(offer.ticket, first, timing))
            return false;
        reading_ = false;
        offer_ = current;
        word_ = current.word;
        return true;
    }

    bool DspWordTransport::beginWrite(CommandSession& session, const DspOffer& offer, NativeFrame now,
                                     BusCycle first, HdiWordTiming timing) noexcept
    {
        DspTransactionProgress progress;
        return session.transactions().progress(offer.ticket, progress) &&
            progress.address.mode == DspAddressMode::None && beginOffered(session, offer, now, first, timing);
    }

    bool DspWordTransport::beginAddressed(CommandSession& session, const DspOffer& offer, NativeFrame now,
                                         BusCycle first, HdiWordTiming timing, DspWordAvailability availability) noexcept
    {
        DspTransactionProgress progress;
        if (offer.offset || !session.transactions().progress(offer.ticket, progress) ||
            progress.address.mode == DspAddressMode::None ||
            (progress.address.mode == DspAddressMode::Read && !session.feedbackRoom(offer.ticket.session, endpoint_)) ||
            !beginOffered(session, offer, now, first, timing))
            return false;
        model_ = progress.model;
        addressMode_ = progress.address.mode;
        wireWordCount_ = progress.wordCount;
        availability_ = availability;
        action_ = Action::SampleAddressReady;
        return true;
    }

    bool DspWordTransport::beginReply(CommandSession& session, DspTicket ticket, BusCycle first,
                                     HdiWordTiming timing) noexcept
    {
        DspTransactionProgress progress;
        if (!session.feedbackRoom(ticket.session, endpoint_) || !session.transactions().progress(ticket, progress) ||
            progress.status != DspStatus::Transferred || progress.releaseEndpoint != DspMilestone::Observation ||
            progress.address.mode != DspAddressMode::None ||
            !begin(ticket, first, timing))
            return false;
        reading_ = true;
        word_ = 0;
        return true;
    }

    bool DspWordTransport::scheduleAfter(BusCycle now, uint64_t delay) noexcept
    {
        if (delay > LastTick - now.ticks)
            return false;
        next_ = {now.ticks + delay};
        return true;
    }

    bool DspWordTransport::scheduleWait(Action action, BusCycle now, uint64_t delay) noexcept
    {
        if (!scheduleAfter(now, delay ? delay : timing_.strobeSpacing) || timing_.readyTimeout > LastTick - next_.ticks)
            return false;
        action_ = action;
        expires_ = {next_.ticks + timing_.readyTimeout};
        blockedSince_.reset();
        return true;
    }

    std::optional<BusCycle> DspWordTransport::nextBusAccess() const noexcept
    {
        return action_ == Action::Idle || action_ == Action::DeliverReply ? std::nullopt : std::optional{next_};
    }

    std::optional<DspTicket> DspWordTransport::activeTicket() const noexcept
    {
        return action_ == Action::Idle ? std::nullopt : std::optional{ticket_};
    }

    HdiStrobe DspWordTransport::nextStrobe() const noexcept
    {
        switch (action_)
        {
        case Action::HighStrobe: return HdiStrobe::High;
        case Action::MiddleStrobe: return HdiStrobe::Middle;
        case Action::LowStrobe: return HdiStrobe::Low;
        default: return HdiStrobe::None;
        }
    }

    bool DspWordTransport::retained(CommandSession& session) const noexcept
    {
        DspTransactionProgress progress;
        if (!session.transactions().progress(ticket_, progress) || session.transactions().poisoned(endpoint_))
            return false;
        if (addressAccepted_)
            return progress.accepted == 1 && (wireWordCount_ == 1 ?
                progress.status == DspStatus::Transferred : progress.status == DspStatus::Queued);
        return reading_ ? progress.status == DspStatus::Transferred :
            progress.status == DspStatus::Queued && progress.accepted == offer_.offset;
    }

    DspCancellation DspWordTransport::cancel(CommandSession& session) noexcept
    {
        if (action_ == Action::Idle || session.snapshot().session != ticket_.session)
            return DspCancellation::Rejected;
        action_ = Action::Idle;
        return session.transactions().cancel(ticket_);
    }

    HdiWordStatus DspWordTransport::fail(CommandSession& session, HdiWordStatus reason) noexcept
    {
        cancel(session);
        if (reason == HdiWordStatus::Timeout)
            ++metrics_.timeouts;
        else
            ++metrics_.faults;
        return reason;
    }

    HdiWordStatus DspWordTransport::deliver(CommandSession& session)
    {
        const auto result = session.receiveReply(ticket_, replyTime_, word_);
        if (result.status == AdmissionStatus::Full)
            return HdiWordStatus::WaitingForFeedback;
        if (result.status != AdmissionStatus::Accepted)
            return fail(session, HdiWordStatus::Fault);
        action_ = Action::Idle;
        ++metrics_.replies;
        if (addressMode_ != DspAddressMode::None)
            ++metrics_.transactions;
        return HdiWordStatus::ReplyQueued;
    }

    void DspWordTransport::finishWait(BusCycle now, bool ready) noexcept
    {
        if (!blockedSince_)
            return;
        metrics_.blockedCycles += now.ticks - blockedSince_->ticks;
        metrics_.readinessTransitions += ready;
        blockedSince_.reset();
    }

    HdiWordStatus DspWordTransport::waitForPort(CommandSession& session, BusCycle now, bool ready) noexcept
    {
        if (ready)
        {
            finishWait(now, true);
            return HdiWordStatus::Progress;
        }
        const bool firstPoll = !blockedSince_;
        if (firstPoll)
            blockedSince_ = now;
        if (!(now < expires_))
        {
            finishWait(now, false);
            return fail(session, HdiWordStatus::Timeout);
        }
        const auto interval = firstPoll && timing_.firstBusyPollInterval ? timing_.firstBusyPollInterval : timing_.pollInterval;
        next_ = {now.ticks + std::min(interval, expires_.ticks - now.ticks)};
        return HdiWordStatus::WaitingForReadiness;
    }

    HdiWordStatus DspWordTransport::nextPayload(CommandSession& session, BusCycle now, NativeFrame native) noexcept
    {
        const auto next = session.transactions().offer(ticket_.session, endpoint_, native);
        if (next.status != DspOfferStatus::Ready || next.ticket.sequence != ticket_.sequence ||
            next.offset != offer_.offset + 1)
            return fail(session, HdiWordStatus::Fault);
        offer_ = next;
        word_ = next.word;
        addressAccepted_ = false;
        retryLow_ = false;
        if (availability_.next)
        {
            const auto due = availability_.next(availability_.context, ticket_, offer_.offset, now);
            if (!due || due->ticks <= now.ticks || due->ticks - now.ticks < timing_.strobeSpacing)
                return fail(session, HdiWordStatus::Fault);
            return scheduleWait(Action::SampleStatus, now, due->ticks - now.ticks)
                ? HdiWordStatus::Progress : fail(session, HdiWordStatus::Fault);
        }
        return scheduleWait(Action::SampleStatus, now) ? HdiWordStatus::Progress : fail(session, HdiWordStatus::Fault);
    }

    HdiWordStatus DspWordTransport::advanceAddress(CommandSession& session, BusCycle now, NativeFrame native) noexcept
    {
        ++metrics_.addressSelections;
        if (addressMode_ == DspAddressMode::Read)
        {
            reading_ = true;
            word_ = 0;
            action_ = Action::HighStrobe;
            return scheduleAfter(now, timing_.strobeSpacing) ? HdiWordStatus::Progress : fail(session, HdiWordStatus::Fault);
        }
        if (wireWordCount_ != 1)
            return nextPayload(session, now, native);
        if (!session.transactions().recordObserved(ticket_))
            return fail(session, HdiWordStatus::Fault);
        action_ = Action::Idle;
        ++metrics_.transactions;
        return HdiWordStatus::AddressSelected;
    }

    HdiWordStatus DspWordTransport::poll(CommandSession& session, BusCycle now, NativeFrame native)
    {
        if (expires_ < now)
        {
            finishWait(now, false);
            return fail(session, HdiWordStatus::Timeout);
        }
        lastAccess_ = now;
        ++metrics_.statusReads;
        const auto status = port_.read(HdiRegister::Status);
        bool ready = false;
        Action next = Action::HighStrobe;
        switch (action_)
        {
        case Action::SampleAddressReady:
            ready = (status & TxReady) != 0;
            next = Action::AssertAddressFlag;
            break;
        case Action::SampleKFlagSet:
            ready = (status & DspHf3) != 0;
            next = Action::ReleaseAddressFlag;
            break;
        case Action::SampleAddressFence:
            ready = addressMode_ == DspAddressMode::Read ? (status & RxFull) != 0 :
                model_ == Model::K ? (status & DspHf3) == 0 : (status & TxReady) != 0;
            next = Action::ReleaseAddressFlag;
            break;
        default:
            ready = (status & (reading_ ? RxFull :
                addressMode_ != DspAddressMode::None && !offer_.offset ? TxReady : TxEmpty)) != 0;
            next = retryLow_ ? Action::LowStrobe : Action::HighStrobe;
            break;
        }
        const auto wait = waitForPort(session, now, ready);
        if (!ready)
            return wait;
        if (action_ == Action::SampleAddressFence && model_ == Model::K)
            return advanceAddress(session, now, native);
        action_ = next;
        return scheduleAfter(now, timing_.readyTransitionDelay ? timing_.readyTransitionDelay : timing_.strobeSpacing)
            ? HdiWordStatus::Progress : fail(session, HdiWordStatus::Fault);
    }

    HdiWordStatus DspWordTransport::pump(CommandSession& session, BusCycle now, NativeFrame native) noexcept
    try {
        if (action_ == Action::Idle)
            return HdiWordStatus::Idle;
        const auto owner = session.snapshot();
        if (owner.session != ticket_.session || owner.lifecycle != Lifecycle::Running)
        {
            action_ = Action::Idle;
            if (owner.session != ticket_.session)
                lastAccess_.reset();
            return HdiWordStatus::StaleSession;
        }
        if (!retained(session))
        {
            action_ = Action::Idle;
            ++metrics_.faults;
            return HdiWordStatus::Fault;
        }
        if (!(now < first_))
            metrics_.maxPendingAge = std::max(metrics_.maxPendingAge, now.ticks - first_.ticks);
        if (action_ == Action::DeliverReply)
            return deliver(session);
        if (now < next_)
            return HdiWordStatus::WaitingForBus;
        if (action_ == Action::SampleStatus || action_ == Action::SampleAddressReady ||
            action_ == Action::SampleKFlagSet || action_ == Action::SampleAddressFence)
            return poll(session, now, native);

        if (action_ == Action::AssertAddressFlag || action_ == Action::ReleaseAddressFlag)
        {
            const bool assertFlag = action_ == Action::AssertAddressFlag;
            if (assertFlag && !session.transactions().recordSideEffect(offer_))
                return fail(session, HdiWordStatus::Fault);
            lastAccess_ = now;
            ++metrics_.flagWrites;
            if (!port_.write(HdiRegister::Control, assertFlag ? HostHf0 : 0))
                return fail(session, HdiWordStatus::Fault);
            if (addressAccepted_)
                return advanceAddress(session, now, native);
            const auto next = assertFlag && model_ == Model::K ? Action::SampleKFlagSet : Action::HighStrobe;
            return scheduleWait(next, now, assertFlag && next == Action::HighStrobe ? timing_.addressCommandDelay : 0)
                ? HdiWordStatus::Progress :
                fail(session, HdiWordStatus::Fault);
        }

        const auto action = action_;
        const auto reg = action == Action::HighStrobe ? HdiRegister::High :
            action == Action::MiddleStrobe ? HdiRegister::Middle : HdiRegister::Low;
        const auto shift = action == Action::HighStrobe ? 16 : action == Action::MiddleStrobe ? 8 : 0;
        lastAccess_ = now;
        if (reading_)
        {
            ++metrics_.readStrobes;
            word_ |= uint32_t(port_.read(reg)) << shift;
            if (action == Action::LowStrobe)
            {
                replyTime_ = native;
                action_ = Action::DeliverReply;
                return deliver(session);
            }
        }
        else
        {
            if (action == Action::HighStrobe && !session.transactions().recordSideEffect(offer_))
                return fail(session, HdiWordStatus::Fault);
            ++metrics_.writeStrobes;
            if (!port_.write(reg, uint8_t(word_ >> shift)))
            {
                if (action != Action::LowStrobe)
                    return fail(session, HdiWordStatus::Fault);
                ++metrics_.refusedCommits;
                retryLow_ = true;
                action_ = Action::SampleStatus;
                return waitForPort(session, now, false);
            }
            if (action == Action::LowStrobe)
            {
                if (!session.transactions().recordAccepted(offer_))
                    return fail(session, HdiWordStatus::Fault);
                ++metrics_.acceptedWords;
                if (addressMode_ != DspAddressMode::None)
                {
                    if (!offer_.offset)
                    {
                        addressAccepted_ = true;
                        return scheduleWait(Action::SampleAddressFence, now) ? HdiWordStatus::Progress :
                            fail(session, HdiWordStatus::Fault);
                    }
                    if (offer_.offset + 1 < wireWordCount_)
                        return nextPayload(session, now, native);
                    ++metrics_.transactions;
                }
                action_ = Action::Idle;
                return HdiWordStatus::WordAccepted;
            }
        }
        action_ = action == Action::HighStrobe ? Action::MiddleStrobe : Action::LowStrobe;
        return scheduleAfter(now, timing_.strobeSpacing) ? HdiWordStatus::Progress :
            fail(session, HdiWordStatus::Fault);
    }
    catch (...)
    {
        return fail(session, HdiWordStatus::Fault);
    }
}
