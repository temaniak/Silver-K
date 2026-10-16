#pragma once
#include "ksDspWordTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
// Temporary caller/IRQ ABI projection. The native task owns all transfer data
// and decisions; remove this bridge as remaining callers become native jobs.
class DspWordCalls final {
public:
    static bool candidate(unsigned p){return p>=0x24d8 && p<=0x255e;}
    bool step(ControllerCpu&);
    void resetMetrics(){started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0;
private:
    struct Call {
        DspWordTask task;
        unsigned base,dsp;
        uint32_t callerToken;
        Call(uint32_t word,unsigned entry,unsigned port,uint16_t address,uint32_t token)
            :task(word,entry==0x24d8,address),base(entry),dsp(port),callerToken(token){}
    };
    std::array<std::optional<Call>,16> calls{};
};
using DspWordPartialTest=bool(*)(ControllerCpu&);
void setDspWordPartialForTests(DspWordPartialTest);
}
