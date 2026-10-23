#pragma once
#include <cstdint>
#include <array>
#include <optional>
#include "ksInterruptService.h"
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
// interrupts own typed continuations and never materialize CPU frames.
// The full controller migration remains unfinished.
class RuntimeController final {
public:
    enum class Entry {PostBoot, Reset};
    RuntimeController(const ControllerCpu&,LiveMidiController&,Entry=Entry::PostBoot);
    static void verifyImage(const ControllerCpu&);
    void step(ControllerCpu&);
    void stepStartup(ControllerCpu&);
    void resetMetrics(){steps=interruptEntries=0;nativeInterruptStarts=nativeInterruptReturns=nativeInterruptPhases=0;nativeInterruptInitial=activeNativeInterrupts();nativeInterruptKinds.fill(0);nativeTpuComponents=0;
#if defined(SILVER_KS_CONTROLLER_PROFILING)
        std::fill(boundaryProfile.begin(),boundaryProfile.end(),0);
#endif
    }
    // Exposed for differential verification of individual native boundaries.
    bool stepCode(ControllerCpu&);
    static void arbitrate(ControllerCpu&);
    uint64_t steps=0,interruptEntries=0;
    uint64_t nativeInterruptStarts=0,nativeInterruptReturns=0,nativeInterruptPhases=0,nativeInterruptInitial=0;
    std::array<uint64_t,4> nativeInterruptKinds{};
    uint32_t nativeTpuComponents=0;
    unsigned activeNativeInterrupts() const { return interruptCount; }
    // Const diagnostic of the current typed continuation, separate from the
    // cumulative component-coverage mask and any saved compatibility CPU PC.
    bool nativeTpu0At(Tpu0Service::Component component,bool requireCritical=false) const {
        if(!interruptCount)return false;
        const auto&service=interrupts[interruptCount-1]->service;
        return service.timerAt(component)&&(!requireCritical||service.critical());
    }
    // Diagnostic only; allocated before playback when explicitly enabled.
#if defined(SILVER_KS_CONTROLLER_PROFILING)
    std::vector<uint64_t> boundaryProfile;
#endif
private:
    LiveMidiController& shared;
    struct Interrupt {
        InterruptService service;
        uint8_t previousPriority,priority;
        int depth;
        Interrupt(InterruptService::Kind kind,uint8_t previous,uint8_t admitted,int level)
            :service(kind),previousPriority(previous),priority(admitted),depth(level){}
    };
    // Seven priority levels bound native nesting without dynamic allocation.
    std::array<std::optional<Interrupt>,7> interrupts{};
    unsigned interruptCount=0;
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
