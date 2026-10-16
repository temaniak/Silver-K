#pragma once
#include "ksStatePatchLoadTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StatePatchLoadCalls final {public:static bool candidate(unsigned p){return p>=0x7134&&p<0x7190;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StatePatchLoadTask task;uint32_t token,source,target=0,context=0;Call(StatePatchLoadTask::Entry e,uint32_t t,uint32_t s):task(e),token(t),source(s){}};std::array<std::optional<Call>,16> calls{};};using StatePatchLoadPartialTest=bool(*)(ControllerCpu&);void setStatePatchLoadPartialForTests(StatePatchLoadPartialTest);}
