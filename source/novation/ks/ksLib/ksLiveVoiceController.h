#pragma once
#include "ksControllerState.h"
#include "ksDspWordCalls.h"
#include <cstdint>

namespace ks {
class ControllerCpu;
// Native voice lifecycle and DSP host transport. Parameter calculators and
// periodic voice servicing remain H8; this is deliberately a staged ABI bridge.
class LiveVoiceController final {
public:
    static constexpr bool candidate(unsigned pc){
        return (pc>=0x2416 && pc<0x25fe)||(pc>=0x77dc && pc<0x7872)||
            (pc>=0x8fc0 && pc<0x9374)||(pc>=0xa338 && pc<0xa656)||
            (pc>=0xa67e && pc<0xa69e);
    }
    static void verify(const ControllerCpu&);
    bool step(ControllerCpu&);
    void resetMetrics(){steps=allocationCalls=startCalls=releaseCalls=transportSteps=0;wordCalls.resetMetrics();}
    void discardTransientCalls(){wordCalls.discardCalls();}
    DspWordCalls wordCalls;
    uint64_t steps=0,allocationCalls=0,startCalls=0,releaseCalls=0,transportSteps=0;
};
}
