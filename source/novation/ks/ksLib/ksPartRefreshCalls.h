#pragma once
#include "ksPartRefreshTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class PartRefreshCalls final{public:static bool candidate(unsigned p){return p>=0xa12c&&p<0xa182;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,partialEntries=0;private:struct Call{PartRefreshTask task;uint32_t token;unsigned depth=0;explicit Call(uint32_t t):token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using PartRefreshPartialTest=bool(*)(ControllerCpu&);void setPartRefreshPartialForTests(PartRefreshPartialTest);}
