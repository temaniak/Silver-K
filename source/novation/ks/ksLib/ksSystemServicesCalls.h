#pragma once
#include "ksSystemServicesTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class SystemServicesCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x1306&&pc<0x136e)||(pc>=0x1b74&&pc<0x1b7e)||(pc>=0x2228&&pc<0x2304);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{SystemServicesTask task;uint32_t token;Call(SystemServicesTask::Kind kind,uint32_t token,uint8_t value):task(kind,value),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using SystemServicesPartialTest=bool(*)(ControllerCpu&);
void setSystemServicesPartialForTests(SystemServicesPartialTest);
}
