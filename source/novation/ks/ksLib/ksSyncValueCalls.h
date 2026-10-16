#pragma once
#include "ksSyncValueTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class SyncValueCalls final{public:static bool candidate(unsigned p){return p>=0xa0c4&&p<0xa106;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,partialEntries=0;private:struct Call{SyncValueTask task;uint32_t token;Call(SyncValueTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using SyncValuePartialTest=bool(*)(ControllerCpu&);void setSyncValuePartialForTests(SyncValuePartialTest);}
