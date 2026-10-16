#pragma once
#include "ksGlobalFinishScalarTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class GlobalFinishScalarCalls final{public:static bool candidate(unsigned p){return p>=0x9cac&&p<=0x9cec;}bool step(ControllerCpu&);void retireReturns(const ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;private:struct Call{GlobalFinishScalarTask task;uint32_t token;Call(GlobalFinishScalarTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using GlobalFinishScalarPartialTest=bool(*)(ControllerCpu&);void setGlobalFinishScalarPartialForTests(GlobalFinishScalarPartialTest);}
