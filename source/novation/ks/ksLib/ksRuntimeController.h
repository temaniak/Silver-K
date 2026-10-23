#pragma once
#include <cstdint>
#include <array>
#include <optional>
#include "ksInterruptScheduler.h"
#include "ksSerialForeground.h"
#include "ksStateResetForeground.h"
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
    void resetMetrics(){panelControlsForeground.resetMetrics();panelScanForeground.resetMetrics();stateResetForeground.resetMetrics();serialForeground.resetMetrics();steps=interruptEntries=0;nativeInterruptStarts=nativeInterruptReturns=nativeInterruptPhases=0;nativeInterruptInitial=activeNativeInterrupts();nativeInterruptKinds.fill(0);nativeTpuComponents=nativeAdcComponents=nativeSerialReceiveComponents=0;
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
    StateResetForeground stateResetForeground;
    SerialForeground serialForeground;
    bool activeNativeWork() const {return panelControlsForeground.active()||activeNativeInterrupts()||serialForeground.active()||stateResetForeground.active()||panelScanForeground.active();}
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
