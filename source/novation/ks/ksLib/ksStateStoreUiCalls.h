#pragma once
#include "ksStateStoreUiTask.h"
#include <array>
#include <optional>

namespace ks {
class ControllerCpu;
class StateStoreUiCalls final {
public:
    static bool candidate(unsigned pc){return pc>=0x47b8&&pc<0x4862;}
    bool step(ControllerCpu&);
    void retireReturns(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateStoreUiTask task;uint32_t token;Call(StateStoreUiTask::Kind kind,uint32_t value):task(kind),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateStoreUiPartialTest=bool(*)(ControllerCpu&);
void setStateStoreUiPartialForTests(StateStoreUiPartialTest);
}
