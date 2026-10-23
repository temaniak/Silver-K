#pragma once
#include "ksStateEditWorkflowTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateEditWorkflowCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x4d2c&&pc<0x4f0c);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateEditWorkflowTask task;uint32_t token;uint32_t handler=0;Call(StateEditWorkflowTask::Kind kind,uint32_t token,uint32_t event):task(kind,event),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateEditWorkflowPartialTest=bool(*)(ControllerCpu&);
void setStateEditWorkflowPartialForTests(StateEditWorkflowPartialTest);
}
