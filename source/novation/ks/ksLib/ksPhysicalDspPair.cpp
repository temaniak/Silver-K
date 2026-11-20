#include "ksPhysicalDspPair.h"
#ifdef SILVER_P12_CPU_SCOPES
#include "../../controller/command/p12CpuScopes.h"
#endif
#ifdef SILVER_P12_ACTION_TRACE
#include "../../controller/command/p12ActionTrace.h"
#endif
#ifdef SILVER_P12_NOTE_TRACE
#include "../../controller/command/p12NoteTrace.h"
#endif

#include <stdexcept>

namespace ks {

#ifdef SILVER_P12_NOTE_TRACE
namespace {
void writeDspSnapshot(const std::filesystem::path& directory, DSP& a, DSP& b) {
    std::ofstream meta(directory / "ks-dsp-snapshot.csv");
    meta << "endpoint,frame,cycle,instructions,pc,sr\n";
    for (const auto endpoint : {'a', 'b'}) {
        auto& dsp = endpoint == 'a' ? a : b;
        auto& memory = dsp.memory();
        meta << endpoint << ',' << dsp.audioFramesProduced() << ',' << dsp.cycles() << ','
             << dsp.instructions() << ',' << dsp.core().getPC().toWord() << ','
             << dsp.core().getSR().toWord() << '\n';
        constexpr std::array areas{dsp56k::MemArea_P, dsp56k::MemArea_X, dsp56k::MemArea_Y};
        constexpr std::array names{'p', 'x', 'y'};
        for (size_t i = 0; i < areas.size(); ++i) {
            const auto path = directory / (std::string("ks-dsp-") + endpoint + '-' + names[i] + ".u32le");
            std::ofstream data(path, std::ios::binary);
            data.write(reinterpret_cast<const char*>(memory.getMemAreaPtr(areas[i])),
                       std::streamsize(memory.size(areas[i]) * sizeof(dsp56k::TWord)));
            if (!data)
                throw std::runtime_error("Could not write KS DSP snapshot");
        }
    }
    if (!meta)
        throw std::runtime_error("Could not write KS DSP snapshot metadata");
}
}
#endif

PhysicalDspPair::~PhysicalDspPair() = default;

void PhysicalDspPair::resetForStop() {
    disableWorkers();
    for (auto& dsp : dsps) {
        dsp->reset();
        dsp->peripherals().getEsaiClock().setEnabled(false);
        dsp->serialTx = {};
    }
    started.fill(false);
    serialSlaveStarted = false;
    serialNext = serialSlot = 0;
    aToB.reset();
    bToA.reset();
    audio.reset();
}

PhysicalDspPair::PhysicalDspPair() {
    for (auto& dsp : dsps) {
        dsp = std::make_unique<DSP>();
        dsp->peripherals().getEsaiClock().setEnabled(false);
    }
    // B's voice DMA maps the first two parts to TX3 and the next two to TX2.
    // A's effects mixer consumes these in part order across RX2 and RX3.
    dsps[0]->serialRead = [this](dsp56k::Audio::RxFrame& frame) {
        DSP::SerialFrame fromB{};
        if (!bToA.pop(fromB) && serialSlaveStarted)
            ++serialUnderruns[0];
        for (unsigned slot = 0; slot < 2; ++slot) {
            frame[slot][2] = fromB[1 - slot][3];
            frame[slot][3] = fromB[1 - slot][2];
        }
    };
    // A TX1 is the auxiliary DAC output. B RX0 remains its external ADC.
    dsps[1]->serialRead = [this](dsp56k::Audio::RxFrame&) {
        DSP::SerialFrame fromA{};
        if (!aToB.pop(fromA))
            ++serialUnderruns[1];
    };
    dsps[0]->serialWrite = [this](const DSP::SerialFrame& frame) {
        if (serialSlaveStarted && !aToB.push(frame))
            ++serialOverflows[0];
    };
    dsps[1]->serialWrite = [this](const DSP::SerialFrame& frame) {
        if (!bToA.push(frame))
            ++serialOverflows[1];
        AudioFrame output{};
        for (unsigned endpoint = 0; endpoint < 2; ++endpoint) {
            const auto& source = endpoint ? frame : dsps[0]->serialTx;
            for (unsigned lane = 0; lane < 6; ++lane) {
                for (unsigned slot = 0; slot < 2; ++slot) {
                    const auto value = source[slot][lane];
                    output[endpoint * 12 + lane * 2 + slot] =
                        (value & 0x800000) ? int32_t(value) - 0x1000000 : int32_t(value);
                }
            }
        }
#ifdef SILVER_P12_NOTE_TRACE
        auto& trace = novation::controller::command::P12NoteTrace::worker();
        const auto dspFrame = dsps[1]->audioFramesProduced();
        trace.ksSerial(dsps[1]->cycles(), dspFrame, output);
        if (!workers_ && trace.ksSnapshotAt(dspFrame))
            writeDspSnapshot(trace.directory(), *dsps[0], *dsps[1]);
#endif
        audio.push(output);
    };
}

void PhysicalDspPair::enableWorkers(int rate, unsigned block, WorkerDispatch dispatch) {
    if (workers_)
        return;
    if (!startedBoth())
        throw std::logic_error("KS DSP workers require completed sequential boot");
    workers_ = std::make_unique<DspWorkers>(this, [](void* context, unsigned index, int64_t target, bool sampled) {
        static_cast<PhysicalDspPair*>(context)->catchUpDsp(index, target, sampled);
    }, rate, block);
    workerDispatch_ = dispatch;
}

void PhysicalDspPair::catchUpDsp(unsigned index, int64_t target, bool sampled) {
#ifdef SILVER_P12_CPU_SCOPES
    using namespace novation::controller::command::p12;
    CpuScope dspCpu(index ? CpuRegion::DspB : CpuRegion::DspA);
#endif
    using namespace baseLib::performance;
    Scope dspTiming(performance_, index ? Metric::DspB : Metric::DspA, 0, 0, true, sampled);
    dsps[index]->catchUpToCycle(target);
}

void PhysicalDspPair::advanceTo(uint64_t nowController, bool sampled) {
#ifdef SILVER_P12_CPU_SCOPES
    using namespace novation::controller::command::p12;
#endif
    using namespace baseLib::performance;
    if (nowController < controllerCycle_)
        throw std::logic_error("KS DSP pair cannot move its clock backwards");
    controllerCycle_ = nowController;
    const auto serialTime = dspTime(nowController);
    // ESAI clocks are disabled in the DSP executors. Join both catch-ups
    // before running the serial link. Sequential execution keeps B then A.
    // Between link slots the short HDI and controller deadlines still advance
    // both DSPs to the same targets on the coordinator.
    const bool serialBoundary = !serialNext || serialTime >= serialNext;
    if (workers_ && (workerDispatch_ == WorkerDispatch::EveryBoundary || serialBoundary)) {
        const std::array<int64_t, 2> targets{int64_t(serialTime) - epochs[0], int64_t(serialTime) - epochs[1]};
        Scope barrierTiming(performance_, Metric::DspBarrier, 0, 0, true, sampled);
        workers_->run(targets, sampled);
    } else {
        for (unsigned order = 0; order < 2; ++order) {
            const unsigned index = 1 - order;
            if (!dsps[index]->bootFinished())
                continue;
#ifdef SILVER_P12_CPU_SCOPES
            CpuScope dspCpu(index ? CpuRegion::DspB : CpuRegion::DspA);
#endif
            Scope dspTiming(performance_, index ? Metric::DspB : Metric::DspA, 0, 0, true, sampled);
            if (!started[index]) {
                epochs[index] = int64_t(serialTime) - int64_t(dsps[index]->cycles());
                started[index] = true;
                if (index == 1)
                    aToB.reset();
            }
#ifdef SILVER_P12_NOTE_TRACE
            novation::controller::command::P12NoteTrace::worker().ksVoiceDsp = index == 1;
#endif
#ifdef SILVER_P12_ACTION_TRACE
            novation::controller::command::P12ActionTrace::worker().selectKsDsp(dsps[index].get(), index);
#endif
            dsps[index]->catchUpToCycle(int64_t(serialTime) - epochs[index]);
        }
    }

    auto& master = dsps[0]->peripherals().getEsai();
#ifdef SILVER_P12_CPU_SCOPES
    CpuScope serialCpu(CpuRegion::Serial);
#endif
    Scope serialTiming(performance_, Metric::Serial, 0, 0, true, sampled);
    if (!serialNext && master.hasEnabledTransmitters())
        serialNext = serialTime + DSP::SerialSlotCycles;
    while (serialNext && serialTime >= serialNext) {
        // A receives the previous B frame. B then publishes against A's new TX.
        for (unsigned index = 0; index < 2; ++index) {
            auto& esai = dsps[index]->peripherals().getEsai();
            if (index == 1 && !serialSlaveStarted) {
                if ((serialSlot & 1) || !esai.hasEnabledTransmitters())
                    continue;
                serialSlaveStarted = true;
                aToB.reset();
                aToB.push(dsps[0]->serialTx);
            }
            esai.execTX();
            esai.execRX();
        }
        ++serialSlot;
        serialNext += DSP::SerialSlotCycles;
    }
}
}
