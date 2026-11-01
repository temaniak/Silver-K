#pragma once
#include "ksDSP.h"
#include "ksDspWorkers.h"
#include "ksControllerTrace.h"
#include "ksControllerDeviceState.h"
#include "ksBoardComparisonBoundary.h"
#include "ksPlaceholderMidi.h"
#include "ksState.h"
#include "ksPartMidi.h"
#include <vector>
#include <array>
#include <memory>
#include <string>
#include "../../kstation/kstationLib/kstationQueue.h"
#include "../../kstation/kstationLib/kstationMidiQueue.h"
#include "baseLib/performanceDiagnostics.h"

namespace ks {
class BoardNativeOwner;
// Coordinator owns physical resources and typed native execution. DSP workers join each slice.
class Board final : public ControllerResourceBus {
public:
    static constexpr uint32_t ControllerClockHz=25000000;
    // Quotient/remainder avoids an overflowing cycles*Hz intermediate in long sessions.
    static constexpr uint64_t dspTime(uint64_t cycles){return (cycles/ControllerClockHz)*DSP::ClockHz+(cycles%ControllerClockHz)*DSP::ClockHz/ControllerClockHz;}
    static constexpr uint32_t WaveBase=0x860000, WaveSize=129024;
    Board(const std::vector<uint8_t>& os, const std::vector<uint8_t>& bank,const std::vector<uint8_t>& storage={},bool isolateParts=false,bool nativeStartup=false);
    ~Board();
    void advance(uint32_t budget=128);
    void enableDspWorkers(int rate,unsigned block);
    void disableDspWorkers(){dspWorkers.reset();} // Owning worker, between joined slices only.
    void configureDspWorkers(int rate,unsigned block){if(dspWorkers)dspWorkers->configure(rate,block);}
    void setPerformanceDiagnostics(baseLib::performance::Diagnostics* p){performance=p;}
    // Coordinator/quiescent boundaries only. Replay is fixture-only and has no
    // live MIDI/state API; disable by constructing a fresh original-H8 Board.
    void startControllerTrace(size_t events=ControllerTape::MaxEvents,size_t slices=ControllerTape::MaxSlices);
    std::shared_ptr<const ControllerTape> finishControllerTrace();
    void startRecordedController(std::shared_ptr<const ControllerTape>);
    void enableLiveMidiController(bool enabled=true);
    void enableNativeController(bool enabled=true);
    const BoardNativeOwner* nativeOwnerState()const{return nativeOwner.get();}
    uint64_t h8InterpretedSteps()const{return comparison?comparison->interpretedSteps():0;}
    uint64_t h8StepsSinceNativeHandoff()const{return comparison?comparison->stepsSinceHandoff():0;}
    void enableControllerDelegationProfile(){if(comparison)comparison->enableDelegationProfile();}
    void resetControllerMetrics();
    const char* controllerBackend()const{return comparison?comparison->name():"native-cpp-semantic";}
    uint64_t controllerCycles()const{return comparison?comparison->cycles():device->cycles;}
    uint64_t checkedControllerPorts()const{return comparison?comparison->checkedPorts():0;}
    // Coordinator only, between advance() calls. Host threads use Device's
    // event queue; the parser/placeholder state is not a concurrent API.
    bool enqueueMidi(const uint8_t* b,size_t n);
    bool enqueuePartMidi(unsigned part,const uint8_t* b,size_t n);
    bool zeroBank()const{return placeholder.enabled;}
    void restoreDesiredLevels(uint8_t* p)const{placeholder.restoreDesired(placeholder.globalChannel,p);}
    bool popMidiOut(uint8_t& b){return midiOut.pop(b);}
    using AudioFrame=std::array<int32_t,24>;
    AudioFrame renderSample();
    uint64_t midiReceived=0;
    uint64_t lastMidiReceiveCycle()const{return midiReceived?rxDue-8000:0;}
    std::array<uint64_t,2> serialUnderruns{},serialOverflows{};
    std::array<size_t,2> serialDepth()const{return {aToB.size(),bToA.size()};}
    uint8_t read(uint32_t address) override;
    void write(uint32_t address,uint8_t value) override;
    ControllerDeviceState& controllerResources(){return *device;}
    const ControllerDeviceState& controllerResources()const{return *device;}
    DSP& dsp(unsigned i){return *dsps.at(i);}
    uint64_t waveReads=0, hostWrites[2]{}, bankLow=0xffffff, bankHigh=0;
    bool overlay=false, mainEntered=false, ready=false;
    bool boot();
    bool demoAvailable()const{return hasDemo;}
    bool demoActive()const{return demoPhase || device->memory[0xa1678c]!=0;}
    // Worker only. Emulate MENUS, then keypad 1+2; PROG stops playback.
    bool setDemoSong(bool enabled);
    bool selectDrumNote(unsigned part,uint8_t note);
    bool drumEditBusy()const{return drumPhase!=0;}
    uint64_t drumEditRevision=0;
    std::vector<uint8_t> saveUserFlash() const;
    void copyUserFlash(std::vector<uint8_t>&) const;
    uint64_t flashRevision=0; // Worker-owned; completed Flash pages/working-map edits.
    State state(); // Worker only: read controller state, never execute guest helpers.
    void discardAudio(){audio.reset();}
    std::string lcd;
private:
    baseLib::performance::Diagnostics* performance=nullptr; // worker-owned attachment
    uint64_t diagnosticAdvances=0;
    bool hasDemo=false;
    uint8_t demoPhase=0;
    uint64_t panelDue=0;
    std::array<uint8_t,6> panelKeys{{255,255,255,255,255,255}};
    void advancePanel();
    uint8_t drumPhase=0,drumPart=0,drumNote=36;
    int previousDrumPart=-1;
    void serviceDrumEdit();
    bool storeDrumEdits(const State&,unsigned part);
    bool drumEditBoundary()const;
    void tickNative(uint64_t previous);
    bool nativeMapping()const;
    bool protectedCode()const;
    uint32_t traceDiagnostic()const;
    void adaptStoredProgram(uint8_t*,size_t);
    void muteStoredPrograms();
    bool enqueueNativeMidi(const uint8_t*,size_t);
    void catchUpDsp(unsigned index,int64_t target,bool sampled);
    // Explicit resource-checkpoint v2 implementation is a separate pending gate.
    uint64_t controllerCheckpoint()const;
    friend class BoardComparisonAccess;
    std::unique_ptr<ControllerDeviceState> nativeDevice;
    ControllerDeviceState* device=nullptr; // borrowed only from concrete comparison owner
    std::unique_ptr<BoardNativeOwner> nativeOwner;
    std::unique_ptr<BoardComparisonBackend> comparison;
    std::array<std::unique_ptr<DSP>,2> dsps;
    std::vector<uint8_t> rom,flash,waves;
    std::array<uint8_t,512> io{};
    int64_t epochs[2]{};
    bool started[2]{};
    uint64_t serialNext=0,serialSlot=0;bool serialSlaveStarted=false;
    uint64_t adcDue=0, rxDue=0,txDue=0;
    // The coordinator fills this UART queue between advance() calls. Snapshot
    // availability once per native slice; only an actual pop consumes a byte.
    uint32_t nativeQueuedMidi=0;
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
    // Destroy/join before any DSP, queue, diagnostics or callback state.
    std::unique_ptr<DspWorkers> dspWorkers;
};
}
