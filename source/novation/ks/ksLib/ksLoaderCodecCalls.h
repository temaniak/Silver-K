#pragma once
#include "ksLoaderCodecTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class LoaderCodecCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0xf38&&pc<0xfbc);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{LoaderCodecTask task;uint32_t token;Call(LoaderCodecTask::Kind kind,uint32_t token,uint16_t word,uint32_t destination,uint32_t sum):task(kind,word,destination,sum),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using LoaderCodecPartialTest=bool(*)(ControllerCpu&);
void setLoaderCodecPartialForTests(LoaderCodecPartialTest);
}
