#pragma once
#include "ksDSP.h"
#include "ksDspWorkers.h"
#include "ksControllerTrace.h"
#include "ksLiveMidiController.h"
#include "ksRuntimeController.h"
#include "ksPlaceholderMidi.h"
#include "ksState.h"
#include "ksPartMidi.h"
#include "h8s/h8s.hpp"
#include <vector>
#include <array>
#include <memory>
#include <string>
#include "../../kstation/kstationLib/kstationQueue.h"
#include "../../kstation/kstationLib/kstationMidiQueue.h"
#include "baseLib/performanceDiagnostics.h"

namespace ks {
// Coordinator owns H8/IO. Optional DSP workers run only inside a joined slice.
class Board final : public H8SDevice {
public:
    static constexpr uint32_t ControllerClockHz=25000000;
    // Quotient/remainder avoids an overflowing cycles*Hz intermediate in long sessions.
    static constexpr uint64_t dspTime(uint64_t cycles){return (cycles/ControllerClockHz)*DSP::ClockHz+(cycles%ControllerClockHz)*DSP::ClockHz/ControllerClockHz;}
    static constexpr uint32_t WaveBase=0x860000, WaveSize=129024;
    Board(const std::vector<uint8_t>& os, const std::vector<uint8_t>& bank,const std::vector<uint8_t>& storage={},bool isolateParts=false);
    void advance(uint32_t budget=128);
    void enableDspWorkers(int rate,unsigned block);
    void configureDspWorkers(int rate,unsigned block){if(dspWorkers)dspWorkers->configure(rate,block);}
    void setPerformanceDiagnostics(baseLib::performance::Diagnostics* p){performance=p;}
    // Coordinator/quiescent boundaries only. Replay is fixture-only and has no
    // live MIDI/state API; disable by constructing a fresh original-H8 Board.
    void startControllerTrace(size_t events=ControllerTape::MaxEvents,size_t slices=ControllerTape::MaxSlices);
    std::shared_ptr<const ControllerTape> finishControllerTrace();
    void startRecordedController(std::shared_ptr<const ControllerTape>);
    void enableLiveMidiController(bool enabled=true);
    void enableNativeController(bool enabled=true);
    const RuntimeController* nativeController()const{return nativeRuntime.get();}
    uint64_t h8InterpretedSteps()const{return h8->interpretedSteps;}
    uint64_t h8StepsSinceNativeHandoff()const{return nativeRuntime?h8->interpretedSteps-nativeH8Steps:0;}
    const LiveMidiController* liveMidiController()const{return liveMidi.get();}
    void enableControllerDelegationProfile(){if(liveMidi)liveMidi->enableDelegationProfile();}
    void resetControllerMetrics(){if(liveMidi)liveMidi->resetMetrics();if(nativeRuntime)nativeRuntime->steps=nativeRuntime->interruptEntries=0;transportJobs=transportBoundaries=0;}
    const char* controllerBackend()const{return recordedController?"recorded-cpp-prototype":nativeRuntime?(eventDspTransport?"native-cpp-events-transport":"native-cpp-runtime"):liveMidi?"live-cpp-midi-hybrid":"original-h8";}
    uint64_t transportJobs=0,transportBoundaries=0;
    uint64_t controllerCycles()const{return recordedController?recordedController->cycles():h8->cycles;}
    uint64_t checkedControllerPorts()const{return recordedController?recordedController->checkedPorts():0;}
    bool enqueueMidi(const uint8_t* b,size_t n);
    bool enqueuePartMidi(unsigned part,const uint8_t* b,size_t n);
    bool zeroBank()const{return placeholder.enabled;}
    void restoreDesiredLevels(uint8_t* p)const{placeholder.restoreDesired(placeholder.globalChannel,p);}
    bool popMidiOut(uint8_t& b){return midiOut.pop(b);}
    using AudioFrame=std::array<int32_t,24>;
    AudioFrame renderSample();
    uint64_t midiReceived=0;
    std::array<uint64_t,2> serialUnderruns{},serialOverflows{};
    std::array<size_t,2> serialDepth()const{return {aToB.size(),bToA.size()};}
    uint8_t read(uint32_t address) override;
    void write(uint32_t address,uint8_t value) override;
    H8SEmulator& cpu(){return *h8;}
    DSP& dsp(unsigned i){return *dsps.at(i);}
    uint64_t waveReads=0, hostWrites[2]{}, bankLow=0xffffff, bankHigh=0;
    bool overlay=false, mainEntered=false, ready=false;
    bool boot();
    std::vector<uint8_t> saveUserFlash() const;
    void copyUserFlash(std::vector<uint8_t>&) const;
    uint64_t flashRevision=0; // Worker-owned; publish completed Flash pages only.
    State state(); // Worker only: read controller state, never execute guest helpers.
    void discardAudio(){audio.reset();}
    std::string lcd;
private:
    baseLib::performance::Diagnostics* performance=nullptr; // worker-owned attachment
    uint64_t diagnosticAdvances=0;
    void tick(uint64_t previous);
    void tickNative(uint64_t previous);
    uint64_t nextControllerEvent(uint64_t limit)const;
    bool eventDspTransport=false;
    void adaptStoredProgram(uint8_t*,size_t);
    void muteStoredPrograms();
    bool enqueueNativeMidi(const uint8_t*,size_t);
    void catchUpDsp(unsigned index,int64_t target,bool sampled);
    uint64_t controllerCheckpoint()const;
    std::unique_ptr<ControllerCpu> h8;
    std::array<std::unique_ptr<DSP>,2> dsps;
    std::vector<uint8_t> rom,flash,waves;
    std::array<uint8_t,512> io{};
    int64_t epochs[2]{};
    bool started[2]{};
    uint64_t serialNext=0,serialSlot=0;bool serialSlaveStarted=false;
    uint64_t adcDue=0, rxDue=0,txDue=0;
    kstation::MidiSerialQueue<65536> midiIn;
    kstation::Queue<uint8_t,65536> midiOut;
    PlaceholderMidi placeholder;
    PartMidi partMidi;
    std::array<std::array<uint8_t,256>,600> programDesired{};
    std::array<std::array<uint8_t,1024>,100> performanceDesired{};
    kstation::Queue<AudioFrame,8192> audio;
    kstation::Queue<DSP::SerialFrame,8192> aToB,bToA;
    uint32_t flashCommand=0,pageRemaining=0;
    std::unique_ptr<ControllerRecorder> controllerRecorder;
    std::unique_ptr<RecordedController> recordedController;
    std::unique_ptr<LiveMidiController> liveMidi;
    std::unique_ptr<RuntimeController> nativeRuntime;
    uint64_t nativeH8Steps=0;
    // Destroy/join before any DSP, queue, diagnostics or callback state.
    std::unique_ptr<DspWorkers> dspWorkers;
};
}
