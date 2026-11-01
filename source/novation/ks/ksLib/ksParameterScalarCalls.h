#pragma once
#include "ksParameterScalarTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class ParameterScalarCalls final {
public:
 static bool candidate(unsigned p);
 void retireReturns(ControllerCpu&);
 bool step(ControllerCpu&);
 unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}
 void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
 struct Call{ParameterScalarTask task;uint32_t token;unsigned binding;Call(ParameterScalarTask::Kind k,uint32_t t,unsigned b):task(k),token(t),binding(b){}};
 std::array<std::optional<Call>,16> calls{};
};
using ParameterScalarPartialTest=bool(*)(ControllerCpu&);
void setParameterScalarPartialForTests(ParameterScalarPartialTest);
}
