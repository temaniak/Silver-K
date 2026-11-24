#include "commandScheduler.h"

#include <algorithm>
#include <atomic>
#include <limits>

namespace novation::controller::command
{
    namespace
    {
        // Allocate only at a cold reset. A delayed reply cannot regain access
        // when a new controller is constructed at the old storage address.
        uint64_t newGeneration() noexcept
        {
            static std::atomic<uint64_t> issued{0};
            auto last = issued.load(std::memory_order_relaxed);
            while (last != std::numeric_limits<uint64_t>::max())
                if (issued.compare_exchange_weak(last, last + 1, std::memory_order_relaxed))
                    return last + 1;
            return 0;
        }

        bool valid(NoteOn event) { return event.channel < 16 && event.note < 128 && event.velocity < 128; }
        bool valid(NoteOff event) { return event.channel < 16 && event.note < 128 && event.velocity < 128; }
        bool valid(ControlChange event) { return event.channel < 16 && event.controller < 128 && event.value < 128; }
        bool valid(ProgramChange event) { return event.channel < 16 && event.program < 128; }
        bool valid(PolyPressure event) { return event.channel < 16 && event.note < 128 && event.pressure < 128; }
        bool valid(ChannelPressure event) { return event.channel < 16 && event.pressure < 128; }
        bool valid(PitchBend event) { return event.channel < 16 && event.value < 16384; }
        bool valid(MidiRealtime event) { return event.status >= 0xf8; }
        bool valid(KSDrumGridEdit event) { return event.valid(); }
        bool valid(KSDesktopSoundCommand event) { return event.request != nullptr; }
        bool valid(KSDrumEditMode event) { return event.valid(); }
        bool valid(KSDrumVoiceEdit event) { return event.valid(); }
        bool valid(MidiCommon event)
        {
            switch (event.status)
            {
            case 0xf1: case 0xf3: return event.data1 < 128 && event.data2 == 0;
            case 0xf2: return event.data1 < 128 && event.data2 < 128;
            case 0xf6: return event.data1 == 0 && event.data2 == 0;
            default: return false;
            }
        }
        bool valid(SysExBytes event)
        {
            return event.data && event.size >= 2 && event.size <= MaxSysExBytes &&
                event.data[0] == 0xf0 && event.data[event.size - 1] == 0xf7 &&
                std::all_of(event.data + 1, event.data + event.size - 1, [](uint8_t byte) { return byte < 128; });
        }
        bool valid(MusicalDeadline event) { return event.kind <= DeadlineKind::ParameterRefresh; }
        bool valid(DspReply event) { return event.endpoint <= Endpoint::B && event.transaction && event.word <= 0xffffff; }
        bool valid(DspWait wait)
        {
            return wait.endpoint <= Endpoint::B && wait.condition <= DspCondition::PhysicalProgress &&
                (wait.condition == DspCondition::ReplyAvailable ? wait.transaction != 0 : wait.transaction == 0);
        }
    }

    bool CommandScheduler::later(const Entry& left, const Entry& right) noexcept
    {
        if (left.due != right.due)
            return right.due < left.due;
        return left.scheduled.sequence > right.scheduled.sequence;
    }

    void CommandScheduler::release(const Entry& entry) noexcept
    {
        if (entry.sysExSlot != NoSlot)
            used_[entry.sysExSlot] = false;
    }

    void CommandScheduler::clear() noexcept
    {
        size_ = 0;
        for (auto& feedback : feedback_)
            feedback.reset();
        inFlight_.reset();
        wait_.reset();
        readyAt_.reset();
        used_.fill(false);
    }

    Session CommandScheduler::reset(NativeFrame origin) noexcept
    {
        if (dispatching_)
            return {};
        const auto generation = newGeneration();
        if (!generation)
            return {};
        clear();
        generation_ = generation;
        sequence_ = wakeAttempt_ = 0;
        now_ = origin;
        lifecycle_ = Lifecycle::Running;
        return session();
    }

    bool CommandScheduler::stop(Session owner) noexcept
    {
        if (owner != session() || lifecycle_ != Lifecycle::Running || dispatching_)
            return false;
        clear();
        lifecycle_ = Lifecycle::Stopped;
        return true;
    }

    Admission CommandScheduler::submit(Session owner, NativeFrame due, ControllerEvent event) noexcept
    {
        if (owner != session())
            return {AdmissionStatus::StaleSession};
        if (lifecycle_ != Lifecycle::Running)
            return {AdmissionStatus::Stopped};
        if (commandCount() == Capacity)
            return {AdmissionStatus::Full};
        if (sequence_ == std::numeric_limits<uint64_t>::max())
            return {AdmissionStatus::SequenceExhausted};
        if (!std::visit([](const auto& payload) { return valid(payload); }, event.payload))
            return {AdmissionStatus::InvalidEvent};
        size_t slot = NoSlot;
        if (auto* bytes = std::get_if<SysExBytes>(&event.payload))
        {
            const auto free = std::find(used_.begin(), used_.end(), false);
            if (free == used_.end())
                return {AdmissionStatus::Full};
            slot = size_t(free - used_.begin());
            std::copy_n(bytes->data, bytes->size, sysEx_[slot].data());
            bytes->data = sysEx_[slot].data();
            used_[slot] = true;
        }
        heap_[size_++] = {{owner, due, ++sequence_, event}, std::max(now_, due), slot};
        std::push_heap(heap_.begin(), heap_.begin() + size_, later);
        return {AdmissionStatus::Accepted, sequence_};
    }

    bool CommandScheduler::feedbackRoom(Session owner, Endpoint endpoint) const noexcept
    {
        return owner == session() && lifecycle_ == Lifecycle::Running && endpoint <= Endpoint::B &&
            !feedback_[size_t(endpoint)] && sequence_ != std::numeric_limits<uint64_t>::max();
    }

    Admission CommandScheduler::submitFeedback(Session owner, NativeFrame due, DspReply reply) noexcept
    {
        if (owner != session())
            return {AdmissionStatus::StaleSession};
        if (lifecycle_ != Lifecycle::Running)
            return {AdmissionStatus::Stopped};
        if (!valid(reply))
            return {AdmissionStatus::InvalidEvent};
        if (sequence_ == std::numeric_limits<uint64_t>::max())
            return {AdmissionStatus::SequenceExhausted};
        auto& mailbox = feedback_[size_t(reply.endpoint)];
        if (mailbox)
            return {AdmissionStatus::Full};
        mailbox = Entry{{owner, due, ++sequence_, {reply}}, std::max(now_, due)};
        return {AdmissionStatus::Accepted, sequence_};
    }

    bool CommandScheduler::cancel(Session owner, uint64_t sequence) noexcept
    {
        if (owner != session() || lifecycle_ != Lifecycle::Running || dispatching_)
            return false;
        if (inFlight_ && inFlight_->scheduled.sequence == sequence)
        {
            release(*inFlight_);
            inFlight_.reset();
            wait_.reset();
            readyAt_.reset();
            return true;
        }
        const auto end = heap_.begin() + size_;
        const auto entry = std::find_if(heap_.begin(), end, [sequence](const Entry& item)
        {
            return item.scheduled.sequence == sequence;
        });
        if (entry == end)
            return false;
        release(*entry);
        *entry = heap_[--size_];
        std::make_heap(heap_.begin(), heap_.begin() + size_, later);
        return true;
    }

    bool CommandScheduler::notifyDsp(WakeToken wake, NativeFrame readyAt) noexcept
    {
        if (!wait_ || readyAt_ || dispatching_ || readyAt < now_ ||
            wake.session != wait_->session || wake.attempt != wait_->attempt ||
            wake.cause.endpoint != wait_->cause.endpoint || wake.cause.condition != wait_->cause.condition ||
            wake.cause.condition == DspCondition::ReplyAvailable || wake.cause.transaction != wait_->cause.transaction)
            return false;
        readyAt_ = readyAt;
        return true;
    }

    std::optional<size_t> CommandScheduler::nextFeedback() const noexcept
    {
        if (!feedback_[0])
            return feedback_[1] ? std::optional<size_t>{1} : std::nullopt;
        if (!feedback_[1])
            return 0;
        const auto& a = *feedback_[0];
        const auto& b = *feedback_[1];
        const auto aTime = std::max(now_, a.due);
        const auto bTime = std::max(now_, b.due);
        return bTime < aTime || (aTime == bTime && b.scheduled.sequence < a.scheduled.sequence) ? 1 : 0;
    }

    std::optional<NativeFrame> CommandScheduler::commandDeadline() const noexcept
    {
        if (wait_)
            return readyAt_;
        if (size_)
            return std::max(now_, heap_[0].due);
        return std::nullopt;
    }

    std::optional<NativeFrame> CommandScheduler::nextDeadline() const noexcept
    {
        const auto command = commandDeadline();
        const auto feedback = nextFeedback();
        if (!feedback)
            return command;
        const auto due = std::max(now_, feedback_[*feedback]->due);
        return !command || due < *command ? due : *command;
    }

    SchedulerSnapshot CommandScheduler::snapshot() const noexcept
    {
        return {session(), lifecycle_, now_, commandCount() + bool(feedback_[0]) + bool(feedback_[1]), nextDeadline(), wait_};
    }

    PumpResult CommandScheduler::runUntil(NativeFrame horizon, unsigned budget, Dispatcher dispatcher) noexcept
    {
        if (lifecycle_ == Lifecycle::Stopped)
            return {PumpStatus::Stopped, 0, 0};
        if (lifecycle_ == Lifecycle::Fault)
            return {PumpStatus::Fault, 0, 0};
        if (horizon < now_ || dispatching_)
            return {PumpStatus::InvalidHorizon, 0, 0};
        unsigned attempts = 0, applied = 0;
        for (;;)
        {
            const auto command = commandDeadline();
            const auto feedback = nextFeedback();
            auto deadline = command;
            bool dispatchFeedback = false;
            if (feedback)
            {
                const auto& entry = *feedback_[*feedback];
                const auto due = std::max(now_, entry.due);
                if (!command || due < *command ||
                    (due == *command && entry.scheduled.sequence <
                        (inFlight_ ? inFlight_->scheduled.sequence : heap_[0].scheduled.sequence)))
                {
                    dispatchFeedback = true;
                    deadline = due;
                }
            }
            if (wait_ && !readyAt_ && (!deadline || horizon < *deadline))
                return {PumpStatus::WaitingForDsp, attempts, applied};
            if (!deadline || horizon < *deadline)
            {
                now_ = horizon;
                return {applied ? PumpStatus::Progress : PumpStatus::WaitingForTime, attempts, applied};
            }
            if (attempts == budget)
                return {PumpStatus::BudgetExhausted, attempts, applied};
            if (dispatchFeedback)
            {
                const auto entry = *feedback_[*feedback];
                const auto reply = std::get<DspReply>(entry.scheduled.event.payload);
                feedback_[*feedback].reset();
                now_ = *deadline;
                dispatching_ = true;
                const auto decision = dispatcher.handle ?
                    dispatcher.handle(dispatcher.context, entry.scheduled, now_) :
                    DispatchDecision{DispatchDecision::Status::Fault, {}};
                dispatching_ = false;
                ++attempts;
                if (decision.status != DispatchDecision::Status::Applied)
                {
                    clear();
                    lifecycle_ = Lifecycle::Fault;
                    return {PumpStatus::Fault, attempts, applied};
                }
                ++applied;
                if (wait_ && wait_->cause.condition == DspCondition::ReplyAvailable &&
                    wait_->cause.endpoint == reply.endpoint && wait_->cause.transaction == reply.transaction)
                    readyAt_ = now_;
                continue;
            }
            if (!inFlight_)
            {
                std::pop_heap(heap_.begin(), heap_.begin() + size_, later);
                inFlight_ = heap_[--size_];
            }
            wait_.reset();
            readyAt_.reset();
            now_ = *deadline;
            dispatching_ = true;
            const auto decision = dispatcher.handle ?
                dispatcher.handle(dispatcher.context, inFlight_->scheduled, now_) :
                DispatchDecision{DispatchDecision::Status::Fault, {}};
            dispatching_ = false;
            ++attempts;
            if (decision.status == DispatchDecision::Status::Applied)
            {
                release(*inFlight_);
                inFlight_.reset();
                ++applied;
                continue;
            }
            if (decision.status == DispatchDecision::Status::WaitForDsp && valid(decision.wait) &&
                wakeAttempt_ != std::numeric_limits<uint64_t>::max())
            {
                wait_ = WakeToken{session(), ++wakeAttempt_, decision.wait};
                return {PumpStatus::WaitingForDsp, attempts, applied};
            }
            clear();
            lifecycle_ = Lifecycle::Fault;
            return {PumpStatus::Fault, attempts, applied};
        }
    }
}
