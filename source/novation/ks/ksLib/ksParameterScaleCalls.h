#pragma once
#include "ksParameterScale.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
// Transitional caller projection; the task owns coefficient/time independently.
// Delete this bridge when the remaining callers use native values directly.
class ParameterScaleCalls final {
public:
    static bool candidate(unsigned p){return p>=0xa69e && p<=0xa6d2;}
    bool step(ControllerCpu&);
    void resetMetrics(){started=completed=partialEntries=0;}
    void discardCalls(){for(auto& c:calls)c.reset();}
    uint64_t started=0,completed=0,partialEntries=0;
private:
    struct Call {
        ParameterScaleTask task;
        PreparedParameterScale prepared;
        int16_t input;
        uint32_t callerToken;
        Call(int16_t encoded,uint32_t token)
            :task(encoded),prepared(prepareParameterScale(encoded)),input(encoded),callerToken(token){}
    };
    std::array<std::optional<Call>,16> calls{};
};
using ParameterScalePartialTest=bool(*)(ControllerCpu&);
void setParameterScalePartialForTests(ParameterScalePartialTest);
}
