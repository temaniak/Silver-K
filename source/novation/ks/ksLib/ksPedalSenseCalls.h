#pragma once
#include "ksPedalSenseTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class PedalSenseCalls final {
public:
    static bool candidate(unsigned pc){return pc>=0x13ae&&pc<0x13f8;}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{PedalSenseTask task;uint32_t token;Call(PedalSenseTask::Kind kind,uint32_t value):task(kind),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using PedalSensePartialTest=bool(*)(ControllerCpu&);
void setPedalSensePartialForTests(PedalSensePartialTest);
}
