#pragma once
#include "ksGlobalTableTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class GlobalTableCalls final{public:static bool candidate(unsigned p){return p>=0x9a50&&p<0x9a7e;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;private:struct Call{GlobalTableTask task;uint32_t token;explicit Call(uint32_t t):token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using GlobalTablePartialTest=bool(*)(ControllerCpu&);void setGlobalTablePartialForTests(GlobalTablePartialTest);}
