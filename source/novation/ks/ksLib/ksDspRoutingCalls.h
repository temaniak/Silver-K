#pragma once
#include "ksDspRoutingTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
// Transitional caller projection; all command preparation lives in the task.
class DspRoutingCalls final {
public:
 static bool candidate(unsigned p){return (p>=0x24b2&&p<=0x24d4)||(p>=0x77dc&&p<=0x7870);}
 bool step(ControllerCpu&);
 void resetMetrics(){started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0;
private:
 struct Call {DspRoutingTask task;uint32_t token;Call(DspRoutingTask::Kind k,uint16_t a,uint32_t t):task(k,a),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using DspRoutingPartialTest=bool(*)(ControllerCpu&);
void setDspRoutingPartialForTests(DspRoutingPartialTest);
}
