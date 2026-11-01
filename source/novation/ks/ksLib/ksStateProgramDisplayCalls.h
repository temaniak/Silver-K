#pragma once
#include "ksStateProgramDisplayTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateProgramDisplayCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x6094&&pc<0x6198);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateProgramDisplayTask task;uint32_t token;Call(StateProgramDisplayTask::Kind kind,uint32_t value):task(kind),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateProgramDisplayPartialTest=bool(*)(ControllerCpu&);
void setStateProgramDisplayPartialForTests(StateProgramDisplayPartialTest);
}
