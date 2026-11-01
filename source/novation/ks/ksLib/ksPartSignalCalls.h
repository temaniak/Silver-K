#pragma once
#include "ksPartSignalTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class PartSignalCalls final{public:static bool candidate(unsigned p){return(p>=0xa106&&p<0xa12c)||(p>=0xa182&&p<0xa1b0);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;private:struct Call{PartSignalTask task;uint32_t token;Call(PartSignalTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using PartSignalPartialTest=bool(*)(ControllerCpu&);void setPartSignalPartialForTests(PartSignalPartialTest);}
