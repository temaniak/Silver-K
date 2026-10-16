#pragma once
#include "ksVelocityCurveTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class VelocityCurveCalls final {
public:
 static bool candidate(unsigned p){return p>=0xa5fc&&p<=0xa654;}
 bool step(ControllerCpu&);
 void resetMetrics(){started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0;
private:
 struct Call {VelocityCurveTask task;uint32_t token;Call(VelocityCurveTask v,uint32_t t):task(v),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using VelocityCurvePartialTest=bool(*)(ControllerCpu&);
void setVelocityCurvePartialForTests(VelocityCurvePartialTest);
}
