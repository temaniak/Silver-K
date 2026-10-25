#pragma once
#include <cstdint>
#include <array>
#include <optional>
#include <memory>
#include "ksInterruptScheduler.h"
#include "ksSerialForeground.h"
#include "ksStateResetForeground.h"
#include "ksStateStoreMenuForeground.h"
#include "ksStateProgramBrowserForeground.h"
#include "ksClockSyncForeground.h"
#include "ksStateDeferredPartsForeground.h"
#include "ksStateRefreshForeground.h"
#include "ksPanelPedalOutputForeground.h"
#include "ksStateEncoderForeground.h"
#include "ksStateTickForeground.h"
#include "ksScheduledTimerForeground.h"
#include "ksNativeMidiAuxForeground.h"
#include "ksNativeArpOrderedForeground.h"
#include "ksNativeArpRunForeground.h"
#include "ksNativePedalControlForeground.h"
#include "ksNativeMidiControlForeground.h"
#include "ksNativePartNoteForeground.h"
#include "ksNativeInternalNoteForeground.h"
#include "ksNativeProgramEventForeground.h"
#include "ksNativeArpEmissionForeground.h"
#include "ksNativeMonoArpBootstrapForeground.h"
#include "ksNativeArpPrimitiveForeground.h"
#include "ksNativeArpGateForeground.h"
#include "ksNativeArpEmitForeground.h"
#include "ksNativeArpChordForeground.h"
#include "ksNativeArpRandomForeground.h"
#include "ksNativeArpSearchForeground.h"
#include "ksPanelMonitorForeground.h"
#include "ksStateStatusForeground.h"
#include "ksDiagnosticCodecForeground.h"
#include "ksDiagnosticTransferForeground.h"
#include "ksMidiFileTimeForeground.h"
#include "ksNativeVoiceReleaseForeground.h"
#include "ksNativeVoiceAllocationForeground.h"
#include "ksNativeVoiceStartForeground.h"
#include "ksNativeVoiceModulationForeground.h"
#include "ksNativeVoiceSweepForeground.h"
#include "ksInternalKeyOutputForeground.h"
#include "ksNativeNoteStateForeground.h"
#include "ksNativeNoteProgramForeground.h"
#include "ksNativeParameterSupportForeground.h"
#include "ksNativeParameterEditForeground.h"
#include "ksPanelMasksForeground.h"
#include "ksVoiceFeedbackForeground.h"
#include "ksPartFeedbackForeground.h"
#include "ksPeriodicForeground.h"
#include "ksStateChannelForeground.h"
#include "ksMidiPacketForeground.h"
#include "ksNativeKeyForeground.h"
#include "ksNativeArpResetForeground.h"
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
private:
    // Fixed native job storage is prepared at controller construction. Large
    // optional UI/voice graphs never consume the caller's thread stack, and
    // starting or advancing a native job performs no allocation.
    struct NativeLargeJobStorage final {
        NativePartNoteForeground partNote;
        NativeProgramEventForeground programEvent;
        ScheduledTimerForeground timers;
        NativeMidiAuxForeground midiAux;
    };
    std::unique_ptr<NativeLargeJobStorage> nativeLargeJobStorage;

public:
    enum class Entry {PostBoot, Reset};
    RuntimeController(const ControllerCpu&,LiveMidiController&,Entry=Entry::PostBoot);
    static void verifyImage(const ControllerCpu&);
    void step(ControllerCpu&);
    void stepStartup(ControllerCpu&);
    void resetMetrics(){scheduledTimerForeground.resetMetrics();nativeMidiAuxForeground.resetMetrics();nativeArpOrderedForeground.resetMetrics();nativeArpRunForeground.resetMetrics();nativePedalControlForeground.resetMetrics();nativeMidiControlForeground.resetMetrics();nativePartNoteForeground.resetMetrics();nativeInternalNoteForeground.resetMetrics();nativeProgramEventForeground.resetMetrics();nativeArpEmissionForeground.resetMetrics();nativeMonoArpBootstrapForeground.resetMetrics();nativeArpPrimitiveForeground.resetMetrics();nativeArpGateForeground.resetMetrics();nativeArpEmitForeground.resetMetrics();nativeArpChordForeground.resetMetrics();nativeArpRandomForeground.resetMetrics();nativeArpSearchForeground.resetMetrics();panelMonitorForeground.resetMetrics();stateStatusForeground.resetMetrics();diagnosticCodecForeground.resetMetrics();diagnosticTransferForeground.resetMetrics();midiFileTimeForeground.resetMetrics();nativeVoiceReleaseForeground.resetMetrics();nativeVoiceAllocationForeground.resetMetrics();nativeVoiceStartForeground.resetMetrics();nativeVoiceModulationForeground.resetMetrics();nativeVoiceSweepForeground.resetMetrics();internalKeyOutputForeground.resetMetrics();nativeNoteStateForeground.resetMetrics();nativeNoteProgramForeground.resetMetrics();nativeParameterSupportForeground.resetMetrics();nativeParameterEditForeground.resetMetrics();panelMasksForeground.resetMetrics();voiceFeedbackForeground.resetMetrics();partFeedbackForeground.resetMetrics();periodicForeground.resetMetrics();stateChannelForeground.resetMetrics();midiPacketForeground.resetMetrics();nativeKeyForeground.resetMetrics();nativeArpResetForeground.resetMetrics();stateTickForeground.resetMetrics();stateEncoderForeground.resetMetrics();panelPedalOutputForeground.resetMetrics();stateRefreshForeground.resetMetrics();stateDeferredPartsForeground.resetMetrics();clockSyncForeground.resetMetrics();stateFactoryForeground.resetMetrics();stateStoreMenuForeground.resetMetrics();stateProgramBrowserForeground.resetMetrics();statePartNavigationForeground.resetMetrics();statePlaybackModeForeground.resetMetrics();stateProgramLoadForeground.resetMetrics();stateEditActionForeground.resetMetrics();stateSceneToggleForeground.resetMetrics();stateSceneEditForeground.resetMetrics();stateVoiceRefreshForeground.resetMetrics();globalUpdateForeground.resetMetrics();parameterUploadForeground.resetMetrics();stateSceneForeground.resetMetrics();stateFieldForeground.resetMetrics();stateScreenForeground.resetMetrics();panelSettingsForeground.resetMetrics();stateSaveForeground.resetMetrics();stateApplyForeground.resetMetrics();stateDisplayForeground.resetMetrics();stateStoreForeground.resetMetrics();stateMenuForeground.resetMetrics();statePanelForeground.resetMetrics();stateEventsForeground.resetMetrics();panelControlsForeground.resetMetrics();panelScanForeground.resetMetrics();stateResetForeground.resetMetrics();serialForeground.resetMetrics();steps=interruptEntries=0;nativeInterruptStarts=nativeInterruptReturns=nativeInterruptPhases=0;nativeInterruptInitial=activeNativeInterrupts();nativeInterruptKinds.fill(0);nativeTpuComponents=nativeAdcComponents=nativeSerialReceiveComponents=0;
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
    ClockSyncForeground clockSyncForeground;
    StateDeferredPartsForeground stateDeferredPartsForeground;
    StateRefreshForeground stateRefreshForeground;
    PanelPedalOutputForeground panelPedalOutputForeground;
    StateEncoderForeground stateEncoderForeground;
    StateTickForeground stateTickForeground;
    ScheduledTimerForeground &scheduledTimerForeground;
    NativeMidiAuxForeground &nativeMidiAuxForeground;
    NativeArpOrderedForeground nativeArpOrderedForeground;
    NativeArpRunForeground nativeArpRunForeground;
    NativePedalControlForeground nativePedalControlForeground;
    NativeMidiControlForeground nativeMidiControlForeground;
    NativePartNoteForeground& nativePartNoteForeground;
    NativeInternalNoteForeground nativeInternalNoteForeground;
    NativeProgramEventForeground& nativeProgramEventForeground;
    NativeArpEmissionForeground nativeArpEmissionForeground;
    NativeMonoArpBootstrapForeground nativeMonoArpBootstrapForeground;
    NativeArpPrimitiveForeground nativeArpPrimitiveForeground;
    NativeArpGateForeground nativeArpGateForeground;
    NativeArpEmitForeground nativeArpEmitForeground;
    NativeArpChordForeground nativeArpChordForeground;
    NativeArpRandomForeground nativeArpRandomForeground;
    NativeArpSearchForeground nativeArpSearchForeground;
    PanelMonitorForeground panelMonitorForeground;
    StateStatusForeground stateStatusForeground;
    DiagnosticCodecForeground diagnosticCodecForeground;
    DiagnosticTransferForeground diagnosticTransferForeground;
    MidiFileTimeForeground midiFileTimeForeground;
    NativeVoiceReleaseForeground nativeVoiceReleaseForeground;
    NativeVoiceAllocationForeground nativeVoiceAllocationForeground;
    NativeVoiceStartForeground nativeVoiceStartForeground;
    NativeVoiceModulationForeground nativeVoiceModulationForeground;
    NativeVoiceSweepForeground nativeVoiceSweepForeground;
    InternalKeyOutputForeground internalKeyOutputForeground;
    NativeNoteStateForeground nativeNoteStateForeground;
    NativeNoteProgramForeground nativeNoteProgramForeground;
    NativeParameterSupportForeground nativeParameterSupportForeground;
    NativeParameterEditForeground nativeParameterEditForeground;
    PanelMasksForeground panelMasksForeground;
    VoiceFeedbackForeground voiceFeedbackForeground;
    PartFeedbackForeground partFeedbackForeground;
    PeriodicForeground periodicForeground;
    StateChannelForeground stateChannelForeground;
    MidiPacketForeground midiPacketForeground;
    NativeKeyForeground nativeKeyForeground;
    NativeArpResetForeground nativeArpResetForeground;
    StateFactoryForeground stateFactoryForeground;
    StateProgramLoadForeground stateProgramLoadForeground;
    StatePlaybackModeForeground statePlaybackModeForeground;
    StatePartNavigationForeground statePartNavigationForeground;
    StateEditActionForeground stateEditActionForeground;
    StateSceneToggleForeground stateSceneToggleForeground;
    StateSceneEditForeground stateSceneEditForeground;
    StateResetForeground stateResetForeground;
    SerialForeground serialForeground;
    bool activeNativeWork() const {return panelMasksForeground.active()||voiceFeedbackForeground.active()||partFeedbackForeground.active()||periodicForeground.active()||stateChannelForeground.active()||midiPacketForeground.active()||nativeKeyForeground.active()||nativeArpResetForeground.active()||nativeVoiceReleaseForeground.active()||nativeVoiceAllocationForeground.active()||nativeVoiceStartForeground.active()||nativeVoiceModulationForeground.active()||nativeVoiceSweepForeground.active()||internalKeyOutputForeground.active()||nativeNoteStateForeground.active()||nativeNoteProgramForeground.active()||nativeParameterSupportForeground.active()||nativeParameterEditForeground.active()||panelMonitorForeground.active()||stateStatusForeground.active()||diagnosticCodecForeground.active()||diagnosticTransferForeground.active()||midiFileTimeForeground.active()||nativePedalControlForeground.active()||nativeMidiControlForeground.active()||nativePartNoteForeground.active()||nativeInternalNoteForeground.active()||nativeProgramEventForeground.active()||nativeArpEmissionForeground.active()||nativeMonoArpBootstrapForeground.active()||nativeArpPrimitiveForeground.active()||nativeArpGateForeground.active()||nativeArpEmitForeground.active()||nativeArpChordForeground.active()||nativeArpRandomForeground.active()||nativeArpSearchForeground.active()||scheduledTimerForeground.active()||nativeMidiAuxForeground.active()||nativeArpOrderedForeground.active()||nativeArpRunForeground.active()||stateTickForeground.active()||stateEncoderForeground.active()||panelPedalOutputForeground.active()||stateRefreshForeground.active()||stateDeferredPartsForeground.active()||clockSyncForeground.active()||stateFactoryForeground.active()||stateStoreMenuForeground.active()||stateProgramBrowserForeground.active()||statePartNavigationForeground.active()||statePlaybackModeForeground.active()||stateProgramLoadForeground.active()||stateEditActionForeground.active()||stateSceneToggleForeground.active()||stateSceneEditForeground.active()||stateVoiceRefreshForeground.active()||globalUpdateForeground.active()||parameterUploadForeground.active()||stateSceneForeground.active()||stateFieldForeground.active()||stateScreenForeground.active()||panelSettingsForeground.active()||stateSaveForeground.active()||stateApplyForeground.active()||stateDisplayForeground.active()||stateStoreForeground.active()||stateMenuForeground.active()||statePanelForeground.active()||stateEventsForeground.active()||panelControlsForeground.active()||activeNativeInterrupts()||serialForeground.active()||stateResetForeground.active()||panelScanForeground.active();}
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
