#pragma once
#include "ksStateEditAddressTask.h"
#include <array>
#include <optional>

namespace ks {
class ControllerCpu;
class StateEditAddressCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x52ac&&pc<0x52e4)||(pc>=0x52e6&&pc<0x530e);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StateEditAddressTask task;uint32_t token;Call(StateEditAddressTask::Kind kind,uint32_t value,uint32_t descriptor):task(kind,descriptor),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StateEditAddressPartialTest=bool(*)(ControllerCpu&);
void setStateEditAddressPartialForTests(StateEditAddressPartialTest);
}
