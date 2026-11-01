#pragma once
#include "ksStateFunctionKeysTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateFunctionKeysCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x4c58&&pc<0x4d2c);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateFunctionKeysTask task;uint32_t token;Call(StateFunctionKeysTask::Kind kind,uint32_t token,uint16_t event,uint32_t record):task(kind,event,record),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateFunctionKeysPartialTest=bool(*)(ControllerCpu&);
void setStateFunctionKeysPartialForTests(StateFunctionKeysPartialTest);
}
