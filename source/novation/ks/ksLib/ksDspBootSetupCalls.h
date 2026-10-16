#pragma once
#include "ksDspBootSetupTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
// Temporary parent/child caller projection; native setup owns its algorithms.
class DspBootSetupCalls final {
public:
    static bool candidate(unsigned p){return p>=0x2332 && p<=0x23cc;}
    bool step(ControllerCpu&);
    void resetMetrics(){started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0;
private:
    struct Call {
        DspBootSetupTask task;
        unsigned dsp;
        uint32_t source,callerToken;
        Call(unsigned port,uint32_t data,uint32_t token):dsp(port),source(data),callerToken(token){}
    };
    std::array<std::optional<Call>,16> calls{};
};
using DspBootSetupPartialTest=bool(*)(ControllerCpu&);
void setDspBootSetupPartialForTests(DspBootSetupPartialTest);
}
