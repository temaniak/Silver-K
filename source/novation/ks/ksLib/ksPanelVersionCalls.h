#pragma once
#include "ksPanelVersionTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class PanelVersionCalls final{public:static bool candidate(unsigned p){return(p>=0xb50&&p<0xb6c)||(p>=0xbda&&p<0xc0e);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,partialEntries=0;private:struct Call{PanelVersionTask task;uint32_t token;Call(PanelVersionTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using PanelVersionPartialTest=bool(*)(ControllerCpu&);void setPanelVersionPartialForTests(PanelVersionPartialTest);}
