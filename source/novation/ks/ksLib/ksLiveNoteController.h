#pragma once
#include "ksControllerState.h"
#include "ksPartControlCalls.h"
#include "ksKeySearchCalls.h"
#include "ksKeyResetCalls.h"
#include <cstdint>

namespace ks {
class ControllerCpu;
// Live note/held-key/pedal state. Kept in the guest ABI during the staged
// migration so native and original handlers can alternate at any boundary.
// DSP voice parameter generation remains in the original voice engine.
class LiveNoteController final {
public:
    static bool candidate(unsigned pc){
        return (pc>=0x2efc && pc<0x31dc)||(pc>=0x33b8 && pc<0x349c)||
            (pc>=0x352c && pc<0x358a)||(pc>=0x3642 && pc<0x36d2)||
            (pc>=0x4000 && pc<0x4016)||(pc>=0x9374 && pc<0x9484)||
            (pc>=0xa656 && pc<0xa67e);
    }
    static void verify(const ControllerCpu&);
    bool step(ControllerCpu&);
    void resetMetrics(){steps=noteOnCalls=noteOffCalls=pedalCalls=pedalReleaseCalls=0;keySearchCalls.resetMetrics();keyResetCalls.resetMetrics();partControlCalls.resetMetrics();}
    void discardTransientCalls(){keySearchCalls.discardCalls();keyResetCalls.discardCalls();partControlCalls.discardCalls();}
    KeySearchCalls keySearchCalls;
    KeyResetCalls keyResetCalls;
    PartControlCalls partControlCalls;
    uint64_t steps=0,noteOnCalls=0,noteOffCalls=0,pedalCalls=0,pedalReleaseCalls=0;
};
}
