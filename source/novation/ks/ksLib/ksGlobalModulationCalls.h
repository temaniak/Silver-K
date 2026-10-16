#pragma once
#include "ksGlobalModulationTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class GlobalModulationCalls final {
public:
 static bool candidate(unsigned p){return p>=0x7a9c&&p<0x7b48;}
 bool step(ControllerCpu&);
 unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}
 void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:
 struct Call{GlobalModulationTask task;uint32_t token;explicit Call(uint32_t t):token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using GlobalModulationPartialTest=bool(*)(ControllerCpu&);
void setGlobalModulationPartialForTests(GlobalModulationPartialTest);
}
