#pragma once
#include "ksStateStoredValueTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateStoredValueCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x57e2&&pc<0x582a);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateStoredValueTask task;uint32_t token;Call(StateStoredValueTask::Kind kind,uint32_t token,uint32_t field):task(kind,field),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateStoredValuePartialTest=bool(*)(ControllerCpu&);
void setStateStoredValuePartialForTests(StateStoredValuePartialTest);
}
