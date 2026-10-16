#pragma once
#include "ksParameterGateTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class ParameterGateCalls final {
public:
 static bool candidate(unsigned);
 bool step(ControllerCpu&);
 void retireReturns(ControllerCpu&);
 unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}
 void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
 struct Call{ParameterGateTask task;uint32_t token;unsigned binding;Call(uint32_t t,unsigned b):token(t),binding(b){}};
 std::array<std::optional<Call>,16> calls{};
};
using ParameterGatePartialTest=bool(*)(ControllerCpu&);
void setParameterGatePartialForTests(ParameterGatePartialTest);
}
