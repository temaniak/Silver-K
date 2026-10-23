#pragma once
#include "ksStateFieldActionsTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateFieldActionsCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x4f0c&&pc<0x5012)||(pc>=0x51d8&&pc<0x52ac)||(pc>=0x52e4&&pc<0x52e6)||(pc>=0x530e&&pc<0x5340);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateFieldActionsTask task;uint32_t token;Call(StateFieldActionsTask::Kind kind,uint32_t token,uint32_t descriptor,uint32_t pointer,uint16_t event,uint16_t mode,uint16_t limits,uint8_t request):task(kind,descriptor,pointer,event,mode,limits,request),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateFieldActionsPartialTest=bool(*)(ControllerCpu&);
void setStateFieldActionsPartialForTests(StateFieldActionsPartialTest);
}
