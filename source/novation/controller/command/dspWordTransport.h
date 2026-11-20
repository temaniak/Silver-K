#pragma once

#include "commandSession.h"
#include "dspEndpoint.h"

namespace novation::controller::command
{
    struct HdiWordTiming
    {
        uint64_t strobeSpacing;
        uint64_t pollInterval;
        uint64_t readyTimeout;
        // Optional host observation policy. Zero retains the basic bus
        // cadence. These are elapsed deadlines between real port actions,
        // not arithmetic work or a firmware instruction cursor.
        uint64_t readyTransitionDelay = 0;
        uint64_t addressCommandDelay = 0;
        uint64_t firstBusyPollInterval = 0;
    };

    enum class HdiWordStatus : uint8_t
    {
        Idle, Progress, WaitingForBus, WaitingForReadiness, WaitingForFeedback,
        WordAccepted, ReplyQueued, AddressSelected, Timeout, Fault, StaleSession
    };

    enum class HdiStrobe : uint8_t { None, High, Middle, Low };

    struct DspWordAvailability
    {
        void* context = nullptr;
        // Called once for each new addressed payload word, after the previous
        // physical milestone: address flag/fence completion for offset one,
        // accepted low-byte commit for later words. The returned absolute
        // cycle is the earliest live readiness observation for that word.
        // The owner/context must outlive the transaction and retain the
        // dependency snapshot used to calculate the immutable payload.
        // Offset includes the address word (first payload offset is one).
        std::optional<BusCycle> (*next)(void*, DspTicket, size_t offset, BusCycle previous) noexcept = nullptr;
    };

    struct HdiWordMetrics
    {
        uint64_t statusReads = 0;
        uint64_t writeStrobes = 0;
        uint64_t readStrobes = 0;
        uint64_t acceptedWords = 0;
        uint64_t refusedCommits = 0;
        uint64_t replies = 0;
        uint64_t timeouts = 0;
        uint64_t faults = 0;
        uint64_t addressSelections = 0;
        uint64_t flagWrites = 0;
        uint64_t transactions = 0;
        uint64_t readinessTransitions = 0;
        uint64_t blockedCycles = 0;
        uint64_t maxPendingAge = 0;
    };

    // The coordinator creates one instance per physical host port. A pump
    // call performs at most one actual
    // register access. The owner provides bus timing, separate from native
    // audio time, and advances DSPs outside this class.
    class DspWordTransport final
    {
    public:
        DspWordTransport(Endpoint endpoint, DspEndpoint port) noexcept : endpoint_(endpoint), port_(port) {}
        DspWordTransport(const DspWordTransport&) = delete;
        DspWordTransport& operator=(const DspWordTransport&) = delete;

        bool beginWrite(CommandSession& session, const DspOffer& offer, NativeFrame now,
                        BusCycle first, HdiWordTiming timing) noexcept;
        bool beginAddressed(CommandSession& session, const DspOffer& offer, NativeFrame now,
                            BusCycle first, HdiWordTiming timing, DspWordAvailability availability = {}) noexcept;
        bool beginReply(CommandSession& session, DspTicket ticket, BusCycle first, HdiWordTiming timing) noexcept;
        HdiWordStatus pump(CommandSession& session, BusCycle now, NativeFrame native) noexcept;
        DspCancellation cancel(CommandSession& session) noexcept;
        std::optional<DspTicket> activeTicket() const noexcept;
        HdiStrobe nextStrobe() const noexcept;
        bool reading() const noexcept { return reading_; }
        std::optional<BusCycle> nextBusAccess() const noexcept;
        HdiWordMetrics metrics() const noexcept { return metrics_; }

    private:
        enum class Action : uint8_t
        {
            Idle, SampleStatus, SampleAddressReady, AssertAddressFlag, SampleKFlagSet,
            ReleaseAddressFlag, SampleAddressFence, HighStrobe, MiddleStrobe, LowStrobe, DeliverReply
        };

        bool begin(DspTicket ticket, BusCycle first, HdiWordTiming timing) noexcept;
        bool beginOffered(CommandSession& session, const DspOffer& offer, NativeFrame now,
                          BusCycle first, HdiWordTiming timing) noexcept;
        bool scheduleAfter(BusCycle now, uint64_t delay) noexcept;
        bool scheduleWait(Action action, BusCycle now, uint64_t delay = 0) noexcept;
        HdiWordStatus waitForPort(CommandSession& session, BusCycle now, bool ready) noexcept;
        void finishWait(BusCycle now, bool ready) noexcept;
        HdiWordStatus advanceAddress(CommandSession& session, BusCycle now, NativeFrame native) noexcept;
        HdiWordStatus nextPayload(CommandSession& session, BusCycle now, NativeFrame native) noexcept;
        HdiWordStatus poll(CommandSession& session, BusCycle now, NativeFrame native);
        HdiWordStatus fail(CommandSession& session, HdiWordStatus reason) noexcept;
        HdiWordStatus deliver(CommandSession& session);
        bool retained(CommandSession& session) const noexcept;

        Endpoint endpoint_;
        DspEndpoint port_;
        DspTicket ticket_{};
        DspOffer offer_{};
        HdiWordTiming timing_{};
        DspWordAvailability availability_{};
        BusCycle next_{};
        BusCycle expires_{};
        BusCycle first_{};
        std::optional<BusCycle> lastAccess_;
        std::optional<BusCycle> blockedSince_;
        NativeFrame replyTime_{};
        HdiWordMetrics metrics_;
        uint32_t word_ = 0;
        size_t wireWordCount_ = 0;
        Action action_ = Action::Idle;
        Model model_ = Model::K;
        DspAddressMode addressMode_ = DspAddressMode::None;
        bool addressAccepted_ = false;
        bool reading_ = false;
        bool retryLow_ = false;
    };
}
