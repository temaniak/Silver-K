#pragma once
#include "ksScheduledTimerTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class ScheduledTimerCalls final {
public:
 static bool candidate(unsigned p){return p>=0x4016&&p<=0x41e0;}
 bool step(ControllerCpu&);void resetMetrics(){started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0;
private:struct Call {ScheduledTimerTask task;uint32_t token;unsigned depth=0;Call(ScheduledTimerTask::Kind k,unsigned s,uint32_t t):task(k,s),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using ScheduledTimerPartialTest=bool(*)(ControllerCpu&);
void setScheduledTimerPartialForTests(ScheduledTimerPartialTest);
}
