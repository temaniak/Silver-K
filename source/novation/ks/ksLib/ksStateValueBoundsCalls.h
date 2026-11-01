#pragma once
#include "ksStateValueBoundsTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateValueBoundsCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x5112&&pc<0x51d8);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateValueBoundsTask task;uint32_t token;Call(StateValueBoundsTask::Kind kind,uint32_t token,uint8_t type,uint8_t value,bool absolute,uint32_t pointer):task(kind,type,value,absolute,pointer),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateValueBoundsPartialTest=bool(*)(ControllerCpu&);
void setStateValueBoundsPartialForTests(StateValueBoundsPartialTest);
}
