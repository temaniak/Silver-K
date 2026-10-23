#pragma once
#include "ksStateForegroundTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateForegroundCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x437a&&pc<0x43a8)||(pc>=0x4406&&pc<0x44cc);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateForegroundTask task;uint32_t token;Call(StateForegroundTask::Kind kind,uint32_t value):task(kind),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateForegroundPartialTest=bool(*)(ControllerCpu&);
void setStateForegroundPartialForTests(StateForegroundPartialTest);
}
