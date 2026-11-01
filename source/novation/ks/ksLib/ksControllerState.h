#pragma once
#include "ksControlArithmetic.h"
#include "ksControllerDeviceState.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
namespace ks {
class ControllerCpu;
// Comparison adapters retain their explicit CPU diagnostic pointer. The real
// resource bus above can be used independently of this compatibility subtype.
class ControllerBus:public ControllerResourceBus {
public:
    void setState(ControllerCpu* p){state=p;}
protected:
    ControllerCpu* state=nullptr;
};
class ControllerReference {
public:
    virtual ~ControllerReference()=default;
    virtual void step(ControllerCpu&)=0;
protected:
    static uint8_t mappedRead(ControllerCpu&,uint32_t);
    static void mappedWrite(ControllerCpu&,uint32_t,uint8_t);
};
using ControllerReferenceFactory=std::unique_ptr<ControllerReference>(*)();
// Only test executables link an implementation. Register once, before main.
void setControllerReferenceFactoryForTests(ControllerReferenceFactory);
bool controllerReferenceAvailable();
// Temporary compatibility state, NOT the completed semantic controller. This
// owns no emulator, opcode decoder or 128 MiB generic memory-map table. Its
// remaining register/flags/stack/continuation ABI still has to be eliminated.
class ControllerCpu final:public ControllerDeviceState {
public:
    ControllerCpu();
    ~ControllerCpu();
    ControllerCpu(const ControllerCpu&)=delete;
    ControllerCpu& operator=(const ControllerCpu&)=delete;
    // Comparison-only adapter. Production has no registered implementation;
    // native execution locks this entry even in a differential-test process.
    void step();
    struct Value {
        union {struct {union {struct {int8_t rl,rh;};int16_t r;};int16_t e;};int32_t er=0;};
    } regs[8]{};
    uint32_t pc=0;
    uint8_t ccr=128,exr=0;
    int indent=0;
    int getPC()const{return int32_t(pc);}
    static uint32_t makepc(uint32_t a){return a;}
    static int pcoff(uint32_t a){return int32_t(a);}
    uint32_t getSP()const{return uint32_t(regs[7].er);}
    void setSP(uint32_t a){regs[7].er=int32_t(a);}
    void memmap(ControllerBus* device,int start,int length=1);
    void boot(){pc=read32(0)&0xffffff;cycles=pending_irqs=pending_irqs_high=0;ccr=128;exr=0;}
    uint64_t interpretedSteps=0;
    bool interpreterAllowed=true;
    bool nativeFetchClocksOnly=false;
    // Native data RAM access avoids the H8 device-map dispatch. MMIO, Flash,
    // waves and protected code writes still use the actual Board callbacks.
    void pushL(uint32_t v){auto sp=getSP()-4;write32(int32_t(v),sp);setSP(sp);}
    void pushW(uint16_t v){auto sp=getSP()-2;write16(int16_t(v),sp);setSP(sp);}
    uint32_t popL(){auto sp=getSP();setSP(sp+4);return read32(sp);}
    uint16_t popW(){auto sp=getSP();setSP(sp+2);return read16(sp);}
    void pushPC(uint32_t target,bool flags=false){auto v=uint32_t(pcoff(target))&0xffffff;pushL(v|(flags?uint32_t(ccr)<<24:0));}
    uint32_t popPC(bool flags=false){auto v=popL();if(flags)ccr=uint8_t(v>>24);return makepc(v&0xffffff);}
    uint32_t loadPCFrom(uint32_t a){return makepc(read32(a)&0xffffff);}
    int8_t add8(int a,int b,int carry=0,bool keepZero=false){
        const auto v=controlArithmetic::add(int8_t(a),int8_t(b),ccr,unsigned(carry),keepZero);ccr=v.flags;return v.value;
    }
    int8_t sub8(int a,int b,int carry=0,bool keepZero=false){
        const auto v=controlArithmetic::subtract(int8_t(a),int8_t(b),ccr,unsigned(carry),keepZero);ccr=v.flags;return v.value;
    }
    int16_t add16(int a,int b){const auto v=controlArithmetic::add(int16_t(a),int16_t(b),ccr);ccr=v.flags;return v.value;}
    int16_t sub16(int a,int b){const auto v=controlArithmetic::subtract(int16_t(a),int16_t(b),ccr);ccr=v.flags;return v.value;}
    int32_t add32(int a,int b,bool noFlags=false){
        const auto v=controlArithmetic::add(int32_t(a),int32_t(b),ccr);if(!noFlags)ccr=v.flags;return v.value;
    }
    int32_t sub32(int a,int b,bool noFlags=false){
        const auto v=controlArithmetic::subtract(int32_t(a),int32_t(b),ccr);if(!noFlags)ccr=v.flags;return v.value;
    }
private:
    friend class ControllerReference;
    friend class RuntimeController;
    friend class StateStoreMenuForeground;
    friend class StateProgramBrowserForeground;
    friend class ClockSyncForeground;
    friend class StateDeferredPartsForeground;
    friend class StateRefreshForeground;
    friend class PanelPedalOutputForeground;
    friend class StateEncoderForeground;
    friend class StateTickForeground;
    friend class NativeMidiPipelineForeground;
    friend class MidiFileEventForeground;
    friend class ServiceCycleForeground;
    friend class NativeArpOwnedForeground;
    friend class MidiFileTransportForeground;
    friend class ScheduledTimerForeground;
    friend class NativeMidiAuxForeground;
    friend class NativeArpOrderedForeground;
    friend class NativeArpRunForeground;
    friend class NativePedalControlForeground;
    friend class NativeMidiControlForeground;
    friend class NativePartNoteForeground;
    friend class NativeInternalNoteForeground;
    friend class NativeProgramEventForeground;
    friend class NativeArpEmissionForeground;
    friend class RoutedEmissionTestBridge;
    friend class NativeMonoArpBootstrapForeground;
    friend class NativeArpPrimitiveForeground;
    friend class NativeArpGateForeground;
    friend class NativeArpEmitForeground;
    friend class NativeArpChordForeground;
    friend class NativeArpRandomForeground;
    friend class NativeArpSearchForeground;
    friend class PanelMonitorForeground;
    friend class StateStatusForeground;
    friend class DiagnosticCodecForeground;
    friend class DiagnosticTransferForeground;
    friend class MidiFileTimeForeground;
    friend class NativeVoiceReleaseForeground;
    friend class NativeVoiceAllocationForeground;
    friend class NativeVoiceStartForeground;
    friend class NativeVoiceModulationForeground;
    friend class NativeVoiceSweepForeground;
    friend class InternalKeyOutputForeground;
    friend class NativeNoteStateForeground;
    friend class NativeNoteProgramForeground;
    friend class NativeParameterSupportForeground;
    friend class NativeParameterEditForeground;
    friend class PanelMasksForeground;
    friend class VoiceFeedbackForeground;
    friend class PartFeedbackForeground;
    friend class PeriodicForeground;
    friend class StateChannelForeground;
    friend class MidiPacketForeground;
    friend class NativeKeyForeground;
    friend class NativeArpResetForeground;
    friend class StateFactoryForeground;
    friend class StateProgramLoadForeground;
    friend class StatePlaybackModeForeground;
    friend class StatePartNavigationForeground;
    friend class StateEditActionForeground;
    friend class StateSceneToggleForeground;
    friend class StateSceneEditForeground;
    friend class StateSceneForeground;
    friend class StateVoiceRefreshForeground;
    friend class GlobalUpdateForeground;
    friend class ParameterUploadForeground;
    friend class StateScreenForeground;
    friend class PanelSettingsForeground;
    friend class StateFieldForeground;
    friend class StateSaveForeground;
    friend class StateApplyForeground;
    friend class StateDisplayForeground;
    friend class StatePanelForeground;
    friend class StateMenuForeground;
    friend class StateStoreForeground;
    friend class StateEventsForeground;
    friend class PanelControlsForeground;
    friend class PanelScanForeground;
    friend class SerialForeground;
    friend class StateResetForeground;
    std::unique_ptr<ControllerReference> reference;
};
}
