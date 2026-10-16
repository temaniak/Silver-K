#pragma once
#include "ksParameterModulationTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class ParameterModulationCalls final {
public:
 static bool candidate(unsigned p){return (p>=0x7f92&&p<=0x8018)||(p>=0x8206&&p<=0x828c)||(p>=0x847a&&p<=0x8500)||(p>=0x87c4&&p<=0x884a);}
 bool step(ControllerCpu&);
 unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}
 void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
 struct Call{ParameterModulationTask task;uint32_t token;unsigned binding;Call(uint32_t t,unsigned b):task(b),token(t),binding(b){}};
 std::array<std::optional<Call>,16> calls{};
};
using ParameterModulationPartialTest=bool(*)(ControllerCpu&);
void setParameterModulationPartialForTests(ParameterModulationPartialTest);
}
