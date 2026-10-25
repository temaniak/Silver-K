#pragma once
#include <cstdint>
#include <array>
#include <optional>
#include "ksInterruptScheduler.h"
#include "ksSerialForeground.h"
#include "ksStateResetForeground.h"
#include "ksStateStoreMenuForeground.h"
#include "ksStateProgramBrowserForeground.h"
#include "ksStateFactoryForeground.h"
#include "ksStateProgramLoadForeground.h"
#include "ksStatePlaybackModeForeground.h"
#include "ksStatePartNavigationForeground.h"
#include "ksStateEditActionForeground.h"
#include "ksStateSceneToggleForeground.h"
#include "ksStateSceneEditForeground.h"
#include "ksStateVoiceRefreshForeground.h"
#include "ksGlobalUpdateForeground.h"
#include "ksParameterUploadForeground.h"
#include "ksStateSceneForeground.h"
#include "ksStateFieldForeground.h"
#include "ksStateScreenForeground.h"
#include "ksPanelSettingsForeground.h"
#include "ksStateSaveForeground.h"
#include "ksStateApplyForeground.h"
#include "ksStateDisplayForeground.h"
#include "ksStateStoreForeground.h"
#include "ksStateMenuForeground.h"
#include "ksStatePanelForeground.h"
#include "ksStateEventsForeground.h"
#include "ksPanelControlsForeground.h"
#include "ksPanelScanForeground.h"
#if defined(SILVER_KS_CONTROLLER_PROFILING)
#include <vector>
#include <algorithm>
#endif

namespace ks {
class ControllerCpu;
class LiveMidiController;
// Complete post-boot compatibility controller. Fixed C++ control flow consumes
// live MIDI/state and DSP responses; no runtime opcode decoder, tape or JIT.
// Remaining services retain the migration timing/register ABI. Closed native
// IRQ, UART and queue-reset graphs own typed continuations without CPU frames.
// The full controller migration remains unfinished.
class RuntimeController final {
public:
    enum class Entry {PostBoot, Reset};
    RuntimeController(const ControllerCpu&,LiveMidiController&,Entry=Entry::PostBoot);
    static void verifyImage(const ControllerCpu&);
    void step(ControllerCpu&);
    void stepStartup(ControllerCpu&);
    void resetMetrics(){stateFactoryForeground.resetMetrics();stateStoreMenuForeground.resetMetrics();stateProgramBrowserForeground.resetMetrics();statePartNavigationForeground.resetMetrics();statePlaybackModeForeground.resetMetrics();stateProgramLoadForeground.resetMetrics();stateEditActionForeground.resetMetrics();stateSceneToggleForeground.resetMetrics();stateSceneEditForeground.resetMetrics();stateVoiceRefreshForeground.resetMetrics();globalUpdateForeground.resetMetrics();parameterUploadForeground.resetMetrics();stateSceneForeground.resetMetrics();stateFieldForeground.resetMetrics();stateScreenForeground.resetMetrics();panelSettingsForeground.resetMetrics();stateSaveForeground.resetMetrics();stateApplyForeground.resetMetrics();stateDisplayForeground.resetMetrics();stateStoreForeground.resetMetrics();stateMenuForeground.resetMetrics();statePanelForeground.resetMetrics();stateEventsForeground.resetMetrics();panelControlsForeground.resetMetrics();panelScanForeground.resetMetrics();stateResetForeground.resetMetrics();serialForeground.resetMetrics();steps=interruptEntries=0;nativeInterruptStarts=nativeInterruptReturns=nativeInterruptPhases=0;nativeInterruptInitial=activeNativeInterrupts();nativeInterruptKinds.fill(0);nativeTpuComponents=nativeAdcComponents=nativeSerialReceiveComponents=0;
#if defined(SILVER_KS_CONTROLLER_PROFILING)
        std::fill(boundaryProfile.begin(),boundaryProfile.end(),0);
#endif
    }
    // Exposed for differential verification of individual native boundaries.
    bool stepCode(ControllerCpu&);
    static void arbitrate(ControllerCpu&);
    uint64_t steps=0,interruptEntries=0;
    uint64_t nativeInterruptStarts=0,nativeInterruptReturns=0,nativeInterruptPhases=0,nativeInterruptInitial=0;
    std::array<uint64_t,6> nativeInterruptKinds{};
    uint32_t nativeTpuComponents=0,nativeAdcComponents=0,nativeSerialReceiveComponents=0;
    PanelScanForeground panelScanForeground;
    PanelControlsForeground panelControlsForeground;
    StateEventsForeground stateEventsForeground;
    StatePanelForeground statePanelForeground;
    StateMenuForeground stateMenuForeground;
    StateStoreForeground stateStoreForeground;
    StateDisplayForeground stateDisplayForeground;
    StateApplyForeground stateApplyForeground;
    StateSaveForeground stateSaveForeground;
    PanelSettingsForeground panelSettingsForeground;
    StateScreenForeground stateScreenForeground;
    StateFieldForeground stateFieldForeground;
    StateSceneForeground stateSceneForeground;
    ParameterUploadForeground parameterUploadForeground;
    GlobalUpdateForeground globalUpdateForeground;
    StateVoiceRefreshForeground stateVoiceRefreshForeground;
    StateStoreMenuForeground stateStoreMenuForeground;
    StateProgramBrowserForeground stateProgramBrowserForeground;
    StateFactoryForeground stateFactoryForeground;
    StateProgramLoadForeground stateProgramLoadForeground;
    StatePlaybackModeForeground statePlaybackModeForeground;
    StatePartNavigationForeground statePartNavigationForeground;
    StateEditActionForeground stateEditActionForeground;
    StateSceneToggleForeground stateSceneToggleForeground;
    StateSceneEditForeground stateSceneEditForeground;
    StateResetForeground stateResetForeground;
    SerialForeground serialForeground;
    bool activeNativeWork() const {return stateFactoryForeground.active()||stateStoreMenuForeground.active()||stateProgramBrowserForeground.active()||statePartNavigationForeground.active()||statePlaybackModeForeground.active()||stateProgramLoadForeground.active()||stateEditActionForeground.active()||stateSceneToggleForeground.active()||stateSceneEditForeground.active()||stateVoiceRefreshForeground.active()||globalUpdateForeground.active()||parameterUploadForeground.active()||stateSceneForeground.active()||stateFieldForeground.active()||stateScreenForeground.active()||panelSettingsForeground.active()||stateSaveForeground.active()||stateApplyForeground.active()||stateDisplayForeground.active()||stateStoreForeground.active()||stateMenuForeground.active()||statePanelForeground.active()||stateEventsForeground.active()||panelControlsForeground.active()||activeNativeInterrupts()||serialForeground.active()||stateResetForeground.active()||panelScanForeground.active();}
    unsigned activeNativeInterrupts() const { return interruptScheduler.depth(); }
    // Const diagnostic of the current typed continuation, separate from the
    // cumulative component-coverage mask and any saved compatibility CPU PC.
    bool nativeTpu0At(Tpu0Service::Component component,bool requireCritical=false) const {
        if(!interruptScheduler.active())return false;
        const auto&service=*interruptScheduler.current();
        return service.timerAt(component)&&(!requireCritical||service.critical());
    }
    bool nativeAdcAt(AdcService::Component component) const { return interruptScheduler.active()&&interruptScheduler.current()->adcAt(component); }
    bool nativeSerialReceiveAt(SerialReceiveService::Component component) const { return interruptScheduler.active()&&interruptScheduler.current()->serialReceiveAt(component); }
    // Diagnostic only; allocated before playback when explicitly enabled.
#if defined(SILVER_KS_CONTROLLER_PROFILING)
    std::vector<uint64_t> boundaryProfile;
#endif
private:
    LiveMidiController& shared;
    InterruptScheduler interruptScheduler;
    // Temporary caller-depth projection belongs only to the compatibility edge.
    // The scheduler's priority stack has no CPU depth, register or return token.
    std::array<int,InterruptScheduler::Capacity> compatibilityIrqDepth{};
    bool serviceInterrupt(ControllerCpu&);
    bool startup(ControllerCpu&);
    bool io(ControllerCpu&);
    bool midi(ControllerCpu&);
    bool state(ControllerCpu&);
    bool parameters(ControllerCpu&);
    bool voices(ControllerCpu&);
    bool extended(ControllerCpu&);
};
}
