#pragma once
#include "ksHardwareSetupTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class HardwareSetupCalls final {public:static bool candidate(unsigned p){return (p>=0x456&&p<0x490)||(p>=0x4d6&&p<0x540);}bool step(ControllerCpu&);
void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void discardCalls(){for(auto&c:calls)c.reset();}
uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{HardwareSetupTask task;uint32_t token;Call(HardwareSetupTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};};
using HardwareSetupPartialTest=bool(*)(ControllerCpu&);void setHardwareSetupPartialForTests(HardwareSetupPartialTest);
}
