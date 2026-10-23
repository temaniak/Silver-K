#pragma once
#include "ksPanelAdcScaleTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class PanelAdcScaleCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x176e&&pc<0x1792)||(pc>=0x17a8&&pc<0x180c);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    // completed counts publication handoffs, not returns of the child event task.
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{PanelAdcScaleTask task;uint32_t token,table;Call(PanelAdcScaleTask::Kind kind,uint32_t frame,uint16_t raw,uint8_t channel,uint32_t address):task(kind,raw,channel),token(frame),table(address){}};
    std::array<std::optional<Call>,16> calls_{};
};
using PanelAdcScalePartialTest=bool(*)(ControllerCpu&);
void setPanelAdcScalePartialForTests(PanelAdcScalePartialTest);
}
