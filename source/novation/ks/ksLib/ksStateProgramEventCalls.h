#pragma once
#include "ksStateProgramEventTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateProgramEventCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x61ec&&pc<0x628c);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateProgramEventTask task;uint32_t token;Call(StateProgramEventTask::Kind kind,uint32_t token,uint16_t event):task(kind,event),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateProgramEventPartialTest=bool(*)(ControllerCpu&);
void setStateProgramEventPartialForTests(StateProgramEventPartialTest);
}
