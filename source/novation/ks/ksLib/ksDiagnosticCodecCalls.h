#pragma once
#include "ksDiagnosticCodecTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class DiagnosticCodecCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x1a2c&&pc<0x1ace);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{DiagnosticCodecTask task;uint32_t token;Call(DiagnosticCodecTask::Kind kind,uint32_t token,uint32_t payload,uint32_t source,uint32_t sum,uint16_t weight,uint16_t carry):task(kind,payload,source,sum,weight,carry),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using DiagnosticCodecPartialTest=bool(*)(ControllerCpu&);
void setDiagnosticCodecPartialForTests(DiagnosticCodecPartialTest);
}
