#pragma once
#include "ksPartControlTasks.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class PartControlCalls final {
public:
 static bool candidate(unsigned p){return (p>=0x4000&&p<=0x4014)||(p>=0xa656&&p<=0xa69c);}
 bool step(ControllerCpu&);
 void resetMetrics(){lookupStarted=lookupCompleted=mirrorStarted=mirrorCompleted=partialEntries=0;}
 void discardCalls(){for(auto& c:lookups)c.reset();for(auto& c:mirrors)c.reset();}
 uint64_t lookupStarted=0,lookupCompleted=0,mirrorStarted=0,mirrorCompleted=0,partialEntries=0;
private:
 struct Lookup {PartLookupTask task;uint32_t token;Lookup(PartLookupTask v,uint32_t t):task(v),token(t){}};
 struct Mirror {PartMirrorTask task;uint32_t token;Mirror(uint8_t index,uint32_t t):task(index),token(t){}};
 std::array<std::optional<Lookup>,16> lookups{};std::array<std::optional<Mirror>,16> mirrors{};
};
using PartControlPartialTest=bool(*)(ControllerCpu&);
void setPartControlPartialForTests(PartControlPartialTest);
}
