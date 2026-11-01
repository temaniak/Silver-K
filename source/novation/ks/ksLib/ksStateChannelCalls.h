#pragma once
#include "ksStateChannelTask.h"
#include <array>
#include <optional>

namespace ks {
class ControllerCpu;
class StateChannelCalls final {
public:
    static bool candidate(unsigned pc){return pc>=0x43a8&&pc<0x4406;}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateChannelTask task;uint32_t token;Call(StateChannelTask::Kind kind,uint32_t value):task(kind),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateChannelPartialTest=bool(*)(ControllerCpu&);
void setStateChannelPartialForTests(StateChannelPartialTest);
}
