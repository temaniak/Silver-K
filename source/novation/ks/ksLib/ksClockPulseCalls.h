#pragma once
#include "ksClockPulseTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class ClockPulseCalls final {
public:
    static bool candidate(unsigned pc){return pc>=0x2848&&pc<0x29a6;}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{ClockPulseTask task;uint32_t token;Call(ClockPulseTask::Kind kind,uint32_t value,uint8_t index,uint8_t source):task(kind,index,source),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using ClockPulsePartialTest=bool(*)(ControllerCpu&);
void setClockPulsePartialForTests(ClockPulsePartialTest);
}
