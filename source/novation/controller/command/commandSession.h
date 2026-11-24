#pragma once

#include "commandScheduler.h"
#include "dspTransaction.h"
#include "midiDecoder.h"
#include "preparedMidi.h"

namespace novation::controller::command
{
    class CommandSession;

    struct MusicalDispatch
    {
        void* context = nullptr;
        DispatchDecision (*handle)(void*, CommandSession&, const ScheduledEvent&, NativeFrame) noexcept = nullptr;
    };

    struct HostBlock
    {
        Session session;
        uint64_t rateGeneration;
        HostFrame first;
        uint32_t frames;
    };

    struct MidiAdmission
    {
        Admission admission;
        MidiDecoder::Result decode;
        bool consumed;
    };

    class PreparedCursor
    {
    public:
        PreparedCursor() = default;
        PreparedCursor(const PreparedCursor&) = delete;
        PreparedCursor& operator=(const PreparedCursor&) = delete;

    private:
        friend class CommandSession;
        const PreparedMidi* batch_ = nullptr;
        Session session_{};
        NativeFrame due_{};
        uint8_t part_ = RoutedByChannel;
        size_t next_ = 0;
    };

    struct PreparedDrain
    {
        AdmissionStatus status;
        size_t admitted;
        bool complete;
    };

    // Single-worker boundary. Producers keep immutable prepared batches alive
    // until the cursor completes or is released, then reclaim them off-worker.
    class CommandSession final
    {
    public:
        Session coldReset(Model model, HostFrame host, NativeFrame native, ClockRate hostRate,
                          ClockRate nativeRate, uint32_t lead) noexcept;
        bool stop(Session owner) noexcept;
        bool rebaseHost(Session owner, HostFrame at, ClockRate rate, uint32_t lead) noexcept;
        std::optional<HostBlock> hostBlock(Session owner, HostFrame first, uint32_t frames) const noexcept;
        std::optional<NativeFrame> hostDeadline(HostBlock block, uint32_t offset) const noexcept;

        Admission submit(Session owner, NativeFrame due, ControllerEvent event) noexcept;
        MidiAdmission pushMidiByte(Session owner, NativeFrame due, uint8_t byte) noexcept;
        bool midiAtMessageBoundary() const noexcept { return decoder_.atMessageBoundary(); }
        MidiAdmission pushHostMidiByte(HostBlock block, uint32_t offset, uint8_t byte) noexcept;
        AdmissionStatus beginPrepared(Session owner, PreparedCursor& cursor, const PreparedMidi& batch,
                                      NativeFrame due, uint8_t part = RoutedByChannel) const noexcept;
        AdmissionStatus beginPrepared(Session, PreparedCursor&, const PreparedMidi&&, NativeFrame,
                                      uint8_t = RoutedByChannel) const noexcept = delete;
        PreparedDrain drainPrepared(PreparedCursor& cursor, size_t budget) noexcept;
        bool releasePrepared(PreparedCursor& cursor) const noexcept;
        bool cancel(Session owner, uint64_t sequence) noexcept { return scheduler_.cancel(owner, sequence); }

        bool feedbackRoom(Session owner, Endpoint endpoint) const noexcept;
        Admission receiveReply(DspTicket ticket, NativeFrame observedAt, uint32_t word) noexcept;
        bool notifyDsp(WakeToken wake, NativeFrame readyAt) noexcept;
        PumpResult runUntil(NativeFrame horizon, unsigned budget, MusicalDispatch dispatch) noexcept;

        SchedulerSnapshot snapshot() const noexcept { return scheduler_.snapshot(); }
        uint64_t rateGeneration() const noexcept { return rateGeneration_; }
        Model model() const noexcept { return model_; }
        DspTransactionQueue& transactions() noexcept { return transactions_; }
        const DspTransactionQueue& transactions() const noexcept { return transactions_; }

    private:
        using HostMap = TimeMap<TimeDomain::Host, TimeDomain::Native>;

        AdmissionStatus lifecycle(Session owner) const noexcept;
        bool validPart(uint8_t part) const noexcept;
        bool validEndpoint(Endpoint endpoint) const noexcept;

        CommandScheduler scheduler_;
        DspTransactionQueue transactions_;
        MidiDecoder decoder_;
        Session session_{};
        Model model_ = Model::K;
        std::optional<HostMap> hostMap_;
        ClockRate nativeRate_{0};
        uint32_t lead_ = 0;
        uint64_t rateGeneration_ = 0;
        bool dispatching_ = false;
    };
}
