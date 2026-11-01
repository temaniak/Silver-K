#pragma once
#include "ksPartModulationTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class PartModulationCalls final{public:static bool candidate(unsigned p){return p>=0x9e42&&p<0x9ec8;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;private:struct Call{PartModulationTask task;uint32_t token;Call(PartModulationTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using PartModulationPartialTest=bool(*)(ControllerCpu&);void setPartModulationPartialForTests(PartModulationPartialTest);}
