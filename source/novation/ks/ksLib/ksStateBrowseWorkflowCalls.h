#pragma once
#include "ksStateBrowseWorkflowTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateBrowseWorkflowCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x5db2&&pc<0x5f64);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateBrowseWorkflowTask task;uint32_t token;Call(StateBrowseWorkflowTask::Kind kind,uint32_t token,uint16_t event):task(kind,event),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateBrowseWorkflowPartialTest=bool(*)(ControllerCpu&);
void setStateBrowseWorkflowPartialForTests(StateBrowseWorkflowPartialTest);
}
