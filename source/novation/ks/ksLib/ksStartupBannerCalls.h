#pragma once
#include "ksStartupBannerTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StartupBannerCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0xb6c&&pc<0xbda);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{StartupBannerTask task;uint32_t token;Call(StartupBannerTask::Kind kind,uint32_t value):task(kind),token(value){}};
    std::array<std::optional<Call>,16> calls_{};
};
using StartupBannerPartialTest=bool(*)(ControllerCpu&);
void setStartupBannerPartialForTests(StartupBannerPartialTest);
}
