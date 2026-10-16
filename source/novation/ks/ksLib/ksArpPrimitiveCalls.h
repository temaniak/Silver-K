#pragma once
#include "ksArpPrimitiveTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class ArpPrimitiveCalls final {
public:
 static bool candidate(unsigned p){return (p>=0x36d2&&p<=0x370c)||(p>=0x3a38&&p<=0x3ab0);}
 bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t initialActive=0,started=0,completed=0,partialEntries=0;
private:
 struct Call {ArpPrimitiveTask task;uint32_t token;unsigned depth=0;Call(ArpPrimitiveTask::Kind k,uint8_t part,uint32_t t):task(k,part),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using ArpPrimitivePartialTest=bool(*)(ControllerCpu&);void setArpPrimitivePartialForTests(ArpPrimitivePartialTest);
}
