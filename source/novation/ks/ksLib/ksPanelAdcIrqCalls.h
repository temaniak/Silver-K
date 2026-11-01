#pragma once
#include "ksPanelAdcIrqTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class PanelAdcIrqCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x165c&&pc<0x16a6)||(pc>=0x16e4&&pc<0x176e)||(pc>=0x1792&&pc<0x17a8)||(pc>=0x180c&&pc<0x1848);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;quickReturns=settleReturns=scanReturns=acceptedChannels=previewHandoffs=specialHandoffs=conversionRestarts=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
    uint64_t quickReturns=0,settleReturns=0,scanReturns=0,acceptedChannels=0,previewHandoffs=0,specialHandoffs=0,conversionRestarts=0;
private:
    struct Call{PanelAdcIrqTask task;uint32_t token;explicit Call(uint32_t value):token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using PanelAdcIrqPartialTest=bool(*)(ControllerCpu&);
void setPanelAdcIrqPartialForTests(PanelAdcIrqPartialTest);
}
