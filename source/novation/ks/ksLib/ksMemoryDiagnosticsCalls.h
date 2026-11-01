#pragma once
#include "ksMemoryDiagnosticsTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class MemoryDiagnosticsCalls final {
public:
    static bool candidate(unsigned pc){return (pc>=0x1ace&&pc<0x1b74)||(pc>=0x230a&&pc<0x230c);}
    bool step(ControllerCpu&);
    unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls_)if(c)++n;return n;}
    void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls_)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
    struct Call{MemoryDiagnosticsTask task;uint32_t token;Call(MemoryDiagnosticsTask::Kind kind,uint32_t token,uint32_t base,uint32_t length,uint16_t pattern,uint16_t stride,uint16_t mask,uint8_t mode,uint8_t rounds):task(kind,base,length,pattern,stride,mask,mode,rounds),token(token){}};
    std::array<std::optional<Call>,16> calls_{};
};
using MemoryDiagnosticsPartialTest=bool(*)(ControllerCpu&);
void setMemoryDiagnosticsPartialForTests(MemoryDiagnosticsPartialTest);
}
