#pragma once
#include "ksDspFaultTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
// Temporary caller/IRQ/trace projection for the semantic fault task.
class DspFaultCalls final {
public:
    static bool candidate(unsigned p){return p==0x2304 || p==0x2308 || (p>=0x660 && p<=0x68c);}
    bool step(ControllerCpu&);
    void resetMetrics(){started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0;
private:
    struct Call {DspFaultTask task;uint32_t callerToken;explicit Call(uint32_t token):callerToken(token){}};
    std::array<std::optional<Call>,16> calls{};
};
using DspFaultPartialTest=bool(*)(ControllerCpu&);
void setDspFaultPartialForTests(DspFaultPartialTest);
}
