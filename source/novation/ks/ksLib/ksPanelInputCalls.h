#pragma once
#include "ksPanelInputTask.h"
#include <array>
#include <optional>

namespace ks {
class ControllerCpu;
class PanelInputCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x13fe&&pc<0x1456)||(pc>=0x14e4&&pc<0x1510)||(pc>=0x1578&&pc<0x158c)||(pc>=0x15da&&pc<0x1624)||(pc>=0x1848&&pc<0x1886)||(pc>=0x21e2&&pc<0x2228);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{PanelInputTask task;uint32_t token;Call(PanelInputTask::Kind kind,uint32_t value,uint16_t event):task(kind,event),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using PanelInputPartialTest=bool(*)(ControllerCpu&);
void setPanelInputPartialForTests(PanelInputPartialTest);
}
