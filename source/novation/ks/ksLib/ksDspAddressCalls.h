#pragma once
#include "ksDspAddressTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
// Temporary old-caller/IRQ projection; protocol decisions are task-owned.
class DspAddressCalls final {
public:
    static bool candidate(unsigned p){return (p>=0x2416 && p<=0x248c)||(p>=0x2560 && p<=0x25fc);}
    bool step(ControllerCpu&);
    void resetMetrics(){started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0;
private:
    struct Call {
        DspAddressTask task;
        unsigned base,dsp;
        uint32_t callerToken;
        Call(uint16_t address,unsigned entry,unsigned port,uint32_t token)
            :task(address,entry!=0x2416,entry==0x2560?DspAddressTask::PortSelection::B:entry==0x2568?DspAddressTask::PortSelection::A:DspAddressTask::PortSelection::Keep),
            base(entry==0x2416?0x2416:0x256e),dsp(port),callerToken(token){}
    };
    std::array<std::optional<Call>,16> calls{};
};
using DspAddressPartialTest=bool(*)(ControllerCpu&);
void setDspAddressPartialForTests(DspAddressPartialTest);
}
