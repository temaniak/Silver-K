#pragma once
#include "ksParameterTimeTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class ParameterTimeCalls final {
public:
 static bool candidate(unsigned);
 bool step(ControllerCpu&);
 void retireReturns(ControllerCpu&);
 unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}
 void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
 struct Call{ParameterTimeTask task;uint32_t token;unsigned binding;Call(uint8_t m,uint8_t t,bool o,uint32_t token_,unsigned b):task(m,t,o),token(token_),binding(b){}};
 std::array<std::optional<Call>,16> calls{};
};
using ParameterTimePartialTest=bool(*)(ControllerCpu&);
void setParameterTimePartialForTests(ParameterTimePartialTest);
}
