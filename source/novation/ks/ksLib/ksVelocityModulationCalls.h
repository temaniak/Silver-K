#pragma once
#include "ksVelocityModulationTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class VelocityModulationCalls final {
public:
 static bool candidate(unsigned);
 // A completed inline child is retired without a guest boundary. False then
 // lets the existing caller consume its next boundary in the same step.
 bool step(ControllerCpu&);
 unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}
 void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
 struct Call {VelocityModulationTask task;uint32_t token;unsigned binding;
  Call(bool relative,uint16_t offset,uint32_t t,unsigned b):task(relative,offset),token(t),binding(b){}};
 std::array<std::optional<Call>,16> calls{};
};
using VelocityModulationPartialTest=bool(*)(ControllerCpu&);
void setVelocityModulationPartialForTests(VelocityModulationPartialTest);
}
