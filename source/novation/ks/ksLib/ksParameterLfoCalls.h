#pragma once
#include "ksParameterLfoTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class ParameterLfoCalls final {
public:
 static bool candidate(unsigned);
 bool step(ControllerCpu&);
 void retireReturns(ControllerCpu&);
 unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}
 void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
 struct Call{ParameterLfoTask task;uint32_t token;unsigned binding;Call(uint32_t t,unsigned b):task(b),token(t),binding(b){}};
 std::array<std::optional<Call>,16> calls{};
};
using ParameterLfoPartialTest=bool(*)(ControllerCpu&);
void setParameterLfoPartialForTests(ParameterLfoPartialTest);
}
