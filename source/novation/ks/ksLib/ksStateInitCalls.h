#pragma once
#include "ksStateInitTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StateInitCalls final {public:
 static bool candidate(unsigned p){return p>=0x41e2&&p<=0x4338;}
 bool step(ControllerCpu&);void resetMetrics(){started=completed=partialEntries=0;}
 void discardCalls(){for(auto&c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0;
private:struct Call{StateInitTask task;uint32_t token;explicit Call(uint32_t t):token(t){}};std::array<std::optional<Call>,16>calls{};
};
using StateInitPartialTest=bool(*)(ControllerCpu&);void setStateInitPartialForTests(StateInitPartialTest);
}
