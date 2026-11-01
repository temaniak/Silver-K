#pragma once
#include "ksStateProgramBrowseTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateProgramBrowseCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x5f64&&pc<0x6094);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateProgramBrowseTask task;uint32_t token;Call(StateProgramBrowseTask::Kind kind,uint32_t token,uint16_t request,uint16_t value,uint16_t limit):task(kind,request,value,limit),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateProgramBrowsePartialTest=bool(*)(ControllerCpu&);
void setStateProgramBrowsePartialForTests(StateProgramBrowsePartialTest);
}
