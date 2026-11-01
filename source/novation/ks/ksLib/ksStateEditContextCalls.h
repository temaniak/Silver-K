#pragma once
#include "ksStateEditContextTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateEditContextCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x5a46&&pc<0x5ae2);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateEditContextTask task;uint32_t token;Call(StateEditContextTask::Kind kind,uint32_t value,uint32_t index):task(kind,index),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateEditContextPartialTest=bool(*)(ControllerCpu&);
void setStateEditContextPartialForTests(StateEditContextPartialTest);
}
