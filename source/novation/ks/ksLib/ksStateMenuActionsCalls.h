#pragma once
#include "ksStateMenuActionsTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateMenuActionsCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x4528&&pc<0x4542)||(pc>=0x4698&&pc<0x47b8);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateMenuActionsTask task;uint32_t token;uint32_t handler=0;Call(StateMenuActionsTask::Kind kind,uint32_t token,uint16_t event,bool pressed):task(kind,event,pressed),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateMenuActionsPartialTest=bool(*)(ControllerCpu&);
void setStateMenuActionsPartialForTests(StateMenuActionsPartialTest);
}
