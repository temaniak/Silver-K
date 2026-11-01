#pragma once
#include "ksStateFieldMapTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateFieldMapCalls final {
public:
 static bool candidate(unsigned p){return p>=0x6da4&&p<0x6e0c;}
 bool step(ControllerCpu&);
 unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}
 void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
 struct Call{StateFieldMapTask task;uint32_t token,descriptor=0,base=0;Call(uint8_t p,uint8_t v,uint32_t f,uint32_t t):task(p,v,f),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using StateFieldMapPartialTest=bool(*)(ControllerCpu&);
void setStateFieldMapPartialForTests(StateFieldMapPartialTest);
}
