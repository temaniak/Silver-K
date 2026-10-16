#pragma once
#include "ksParameterPitchTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class ParameterPitchCalls final {
public:
 static bool candidate(unsigned p){return (p>=0x7e8c&&p<=0x7f66)||(p>=0x8100&&p<=0x81da)||(p>=0x8374&&p<=0x844e);}
 bool step(ControllerCpu&);
 unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}
 void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
 struct Call{ParameterPitchTask task;uint32_t token,selectedPatch=0;unsigned binding;Call(uint32_t t,unsigned b):task(b),token(t),binding(b){}};
 std::array<std::optional<Call>,16> calls{};
};
using ParameterPitchPartialTest=bool(*)(ControllerCpu&);
void setParameterPitchPartialForTests(ParameterPitchPartialTest);
}
