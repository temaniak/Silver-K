#pragma once
#include "ksGlobalTimeTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class GlobalTimeCalls final{public:static bool candidate(unsigned p){return (p>=0x97de&&p<=0x983c)||(p>=0x9898&&p<=0x98fe);}void retireReturns(ControllerCpu&);bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0;uint64_t started=0,completed=0,partialEntries=0;private:struct Call{GlobalTimeTask task;uint32_t token;Call(GlobalTimeTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks {using GlobalTimePartialTest=bool(*)(ControllerCpu&);void setGlobalTimePartialForTests(GlobalTimePartialTest); }
