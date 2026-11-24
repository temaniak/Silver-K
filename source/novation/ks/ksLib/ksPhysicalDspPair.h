#pragma once

#include "ksDSP.h"
#include "ksDspWorkers.h"
#include "../../kstation/kstationLib/kstationQueue.h"
#include "baseLib/performanceDiagnostics.h"

namespace ks {

// One clock domain and one serial link for the two DSPs. All access is on
// the coordinator between joined worker slices. This object stays at the
// same address when its owning pointer changes hands.
class PhysicalDspPair final {
public:
    using AudioFrame = std::array<int32_t, 24>;
    enum class WorkerDispatch { EveryBoundary, SerialSlots };
    static constexpr uint32_t ControllerClockHz = 25000000;
    static constexpr uint64_t dspTime(uint64_t cycles) {
        return (cycles / ControllerClockHz) * DSP::ClockHz +
            (cycles % ControllerClockHz) * DSP::ClockHz / ControllerClockHz;
    }

    PhysicalDspPair();
    ~PhysicalDspPair();
    PhysicalDspPair(const PhysicalDspPair&) = delete;
    PhysicalDspPair& operator=(const PhysicalDspPair&) = delete;

    DSP& dsp(unsigned index) { return *dsps.at(index); }
    uint64_t controllerCycle() const { return controllerCycle_; }
    bool startedBoth() const { return started[0] && started[1]; }
    bool serialRunning() const { return serialSlaveStarted; }
    uint64_t nextSerialDspCycle() const { return serialNext; }
    uint64_t serialSlots() const { return serialSlot; }
    std::array<int64_t, 2> dspEpochs() const { return epochs; }
    std::array<size_t, 2> serialDepth() const { return {aToB.size(), bToA.size()}; }
    size_t audioDepth() const { return audio.size(); }
    bool popAudio(AudioFrame& frame) { return audio.pop(frame); }
    void discardAudio() { audio.reset(); }
    void advanceTo(uint64_t nowController, bool sampled = false);
    void resetForStop();

    void setDiagnostics(baseLib::performance::Diagnostics* performance) { performance_ = performance; }
    void enableWorkers(int rate, unsigned block, WorkerDispatch dispatch = WorkerDispatch::EveryBoundary);
    void disableWorkers() { workers_.reset(); }
    void configureWorkers(int rate, unsigned block) { if (workers_) workers_->configure(rate, block); }
    const auto& underruns() const { return serialUnderruns; }
    const auto& overflows() const { return serialOverflows; }

private:
    friend class Board;
    friend class BoardComparisonAccess;

    void catchUpDsp(unsigned index, int64_t target, bool sampled);

    baseLib::performance::Diagnostics* performance_ = nullptr;
    std::array<std::unique_ptr<DSP>, 2> dsps;
    std::array<int64_t, 2> epochs{};
    std::array<bool, 2> started{};
    uint64_t controllerCycle_ = 0;
    uint64_t serialNext = 0, serialSlot = 0;
    bool serialSlaveStarted = false;
    std::array<uint64_t, 2> serialUnderruns{}, serialOverflows{};
    kstation::Queue<AudioFrame, 8192> audio;
    kstation::Queue<DSP::SerialFrame, 8192> aToB, bToA;
    WorkerDispatch workerDispatch_ = WorkerDispatch::EveryBoundary;
    // Join before destroying any resource used by a worker or callback.
    std::unique_ptr<DspWorkers> workers_;
};
}
