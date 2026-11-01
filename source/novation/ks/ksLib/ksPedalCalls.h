#pragma once
#include "ksPedalTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class PedalCalls final {
public:
 static bool candidate(unsigned p){return p>=0x352c&&p<=0x3588;}
 bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t initialActive=0;uint64_t started=0,completed=0,handedOff=0,partialEntries=0;
private:
 struct Call {PedalTask task;uint32_t token;unsigned depth=0;Call(bool checked,uint32_t t):task(checked),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using PedalPartialTest=bool(*)(ControllerCpu&);void setPedalPartialForTests(PedalPartialTest);
}
