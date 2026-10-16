#pragma once
#include "ksGlobalTailScalarTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class GlobalTailScalarCalls final{public:static bool candidate(unsigned p){return p>=0x9bee&&p<=0x9c4e;}bool step(ControllerCpu&);void retireReturns(const ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;private:struct Call{GlobalTailScalarTask task;uint32_t token;Call(GlobalTailScalarTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using GlobalTailScalarPartialTest=bool(*)(ControllerCpu&);void setGlobalTailScalarPartialForTests(GlobalTailScalarPartialTest);}
