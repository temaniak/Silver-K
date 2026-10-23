#pragma once
#include "ksStateScreenEditTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateScreenEditCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x59a6&&pc<0x5a46);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateScreenEditTask task;uint32_t token;Call(StateScreenEditTask::Kind kind,uint32_t value,uint8_t delta):task(kind,delta),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateScreenEditPartialTest=bool(*)(ControllerCpu&);
void setStateScreenEditPartialForTests(StateScreenEditPartialTest);
}
