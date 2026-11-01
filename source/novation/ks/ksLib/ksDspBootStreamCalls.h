#pragma once
#include "ksDspBootStreamTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
// Temporary bootstrap caller/IRQ projection; the stream task owns all work.
class DspBootStreamCalls final {
public:
    static bool candidate(unsigned p){return p>=0x23ce && p<=0x2414;}
    static bool sourceSpanValid(uint32_t source,uint16_t words);
    bool step(ControllerCpu&);
    void resetMetrics(){started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0;
private:
    struct Call {
        DspBootStreamTask task;
        unsigned dsp;
        uint32_t source,callerToken;
        Call(uint16_t words,unsigned port,uint32_t data,uint32_t token):task(words),dsp(port),source(data),callerToken(token){}
    };
    std::array<std::optional<Call>,16> calls{};
};
using DspBootStreamPartialTest=bool(*)(ControllerCpu&);
void setDspBootStreamPartialForTests(DspBootStreamPartialTest);
}
