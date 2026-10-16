#pragma once
#include "ksKeyActivityTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class KeyActivityCalls final {
public:
 static bool candidate(unsigned p){return p>=0x30e4&&p<=0x3116;}
 bool step(ControllerCpu&);
 void resetMetrics(){started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0;
private:
 struct Call {KeyActivityTask task;uint32_t token;Call(KeyActivityTask::Kind k,uint32_t t):task(k),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using KeyActivityPartialTest=bool(*)(ControllerCpu&);
void setKeyActivityPartialForTests(KeyActivityPartialTest);
}
