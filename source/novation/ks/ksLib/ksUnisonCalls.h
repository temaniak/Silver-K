#pragma once
#include "ksUnisonTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class UnisonCalls final {
public:
 static bool candidate(unsigned p){return p>=0x8fcc&&p<=0x9028;}
 bool step(ControllerCpu&);
 void resetMetrics(){started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0;
private:
 struct Call {UnisonTask task;uint32_t token,patch=0;unsigned depth=0;explicit Call(uint32_t t):token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using UnisonPartialTest=bool(*)(ControllerCpu&);
void setUnisonPartialForTests(UnisonPartialTest);
}
