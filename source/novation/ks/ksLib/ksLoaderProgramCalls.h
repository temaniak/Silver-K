#pragma once
#include "ksLoaderProgramTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class LoaderProgramCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0xdfa&&pc<0xe82);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{LoaderProgramTask task;uint32_t token;Call(LoaderProgramTask::Kind kind,uint32_t token,uint16_t pages):task(kind,pages),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using LoaderProgramPartialTest=bool(*)(ControllerCpu&);
void setLoaderProgramPartialForTests(LoaderProgramPartialTest);
}
