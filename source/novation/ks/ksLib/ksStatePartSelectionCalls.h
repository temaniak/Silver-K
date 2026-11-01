#pragma once
#include "ksStatePartSelectionTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StatePartSelectionCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x4542&&pc<0x4698);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StatePartSelectionTask task;uint32_t token;Call(StatePartSelectionTask::Kind kind,uint32_t token,uint16_t event,uint8_t before,uint8_t after):task(kind,event,before,after),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StatePartSelectionPartialTest=bool(*)(ControllerCpu&);
void setStatePartSelectionPartialForTests(StatePartSelectionPartialTest);
}
