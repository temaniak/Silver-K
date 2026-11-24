#pragma once

#include "controllerEvent.h"

#include <array>
#include <optional>

namespace novation::controller::command
{
    enum class AdmissionStatus : uint8_t { Accepted, StaleSession, Stopped, InvalidEvent, Full, SequenceExhausted };
    struct Admission
    {
        AdmissionStatus status;
        uint64_t sequence = 0;
    };

    struct ScheduledEvent
    {
        Session session;
        NativeFrame requestedTime;
        uint64_t sequence;
        ControllerEvent event;
    };

    enum class DspCondition : uint8_t { Writable, ReplyAvailable, PhysicalProgress };
    struct DspWait
    {
        Endpoint endpoint = Endpoint::A;
        DspCondition condition = DspCondition::Writable;
        uint64_t transaction = 0;
    };

    struct WakeToken
    {
        Session session;
        uint64_t attempt;
        DspWait cause;
    };

    struct DispatchDecision
    {
        enum class Status : uint8_t { Applied, WaitForDsp, Fault };
        Status status = Status::Applied;
        DspWait wait;
    };

    struct Dispatcher
    {
        void* context = nullptr;
        DispatchDecision (*handle)(void*, const ScheduledEvent&, NativeFrame) noexcept = nullptr;
    };

    enum class PumpStatus : uint8_t
    {
        Progress, WaitingForTime, WaitingForDsp, BudgetExhausted, Fault, Stopped, InvalidHorizon
    };

    struct PumpResult
    {
        PumpStatus status;
        unsigned attempts;
        unsigned applied;
    };

    enum class Lifecycle : uint8_t { Stopped, Running, Fault };
    struct SchedulerSnapshot
    {
        Session session;
        Lifecycle lifecycle;
        NativeFrame now;
        size_t queued;
        std::optional<NativeFrame> nextDeadline;
        std::optional<WakeToken> wait;
    };

    // One worker owns admission, dispatch and lifecycle. SysEx input is borrowed
    // only during submit; the dispatcher receives a view valid for that call.
    class CommandScheduler final
    {
    public:
        static constexpr size_t Capacity = 256;
        static constexpr size_t SysExSlots = 8;

        CommandScheduler() = default;
        CommandScheduler(const CommandScheduler&) = delete;
        CommandScheduler& operator=(const CommandScheduler&) = delete;

        // Reset is a cold boundary. The caller has already quiesced any
        // physical endpoint that could outlive the previous generation.
        Session reset(NativeFrame origin) noexcept;
        bool stop(Session session) noexcept;
        Admission submit(Session session, NativeFrame due, ControllerEvent event) noexcept;
        Admission submitFeedback(Session session, NativeFrame due, DspReply reply) noexcept;
        bool feedbackRoom(Session session, Endpoint endpoint) const noexcept;
        bool running(Session owner) const noexcept { return owner == session() && lifecycle_ == Lifecycle::Running; }
        bool cancel(Session session, uint64_t sequence) noexcept;
        bool notifyDsp(WakeToken wake, NativeFrame readyAt) noexcept;

        // The budget counts dispatch attempts. A handler returning WaitForDsp
        // has committed neither model state nor a transaction for this event.
        // It may admit follow-up events only when it can return Applied.
        PumpResult runUntil(NativeFrame horizon, unsigned budget, Dispatcher dispatcher) noexcept;
        std::optional<NativeFrame> nextDeadline() const noexcept;
        SchedulerSnapshot snapshot() const noexcept;

    private:
        static constexpr size_t NoSlot = SysExSlots;
        struct Entry
        {
            ScheduledEvent scheduled;
            NativeFrame due;
            size_t sysExSlot = NoSlot;
        };

        static bool later(const Entry& left, const Entry& right) noexcept;
        std::optional<size_t> nextFeedback() const noexcept;
        std::optional<NativeFrame> commandDeadline() const noexcept;
        void release(const Entry& entry) noexcept;
        void clear() noexcept;
        Session session() const noexcept { return {this, generation_}; }
        size_t commandCount() const noexcept { return size_ + bool(inFlight_); }

        std::array<Entry, Capacity> heap_{};
        std::array<std::optional<Entry>, 2> feedback_{};
        std::array<std::array<uint8_t, MaxSysExBytes>, SysExSlots> sysEx_{};
        std::array<bool, SysExSlots> used_{};
        size_t size_ = 0;
        std::optional<Entry> inFlight_;
        std::optional<WakeToken> wait_;
        std::optional<NativeFrame> readyAt_;
        uint64_t generation_ = 0;
        uint64_t sequence_ = 0;
        uint64_t wakeAttempt_ = 0;
        NativeFrame now_{};
        Lifecycle lifecycle_ = Lifecycle::Stopped;
        bool dispatching_ = false;
    };
}
