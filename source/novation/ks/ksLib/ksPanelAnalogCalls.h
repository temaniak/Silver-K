#pragma once
#include "ksPanelAnalogTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class PanelAnalogCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x1510&&pc<0x152e)||(pc>=0x20c8&&pc<0x219a);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{PanelAnalogTask task;uint32_t token;Call(PanelAnalogTask::Kind kind,uint32_t value,uint8_t input,uint8_t selector,uint16_t bias,uint32_t history,uint32_t coefficients):task(kind,input,selector,bias),token(value),history(history),coefficients(coefficients){}uint32_t history,coefficients;};
    std::array<std::optional<Call>,16> calls_{};
};
using PanelAnalogPartialTest=bool(*)(ControllerCpu&);
void setPanelAnalogPartialForTests(PanelAnalogPartialTest);
}
