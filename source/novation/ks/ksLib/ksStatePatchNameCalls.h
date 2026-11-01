#pragma once
#include "ksStatePatchNameTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StatePatchNameCalls final {public:static bool candidate(unsigned p){return p>=0x6e42&&p<0x6ec6;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StatePatchNameTask task;uint32_t token,storage;Call(StatePatchNameTask::Entry e,uint32_t t,uint32_t s):task(e),token(t),storage(s){}};std::array<std::optional<Call>,16> calls{};};using StatePatchNamePartialTest=bool(*)(ControllerCpu&);void setStatePatchNamePartialForTests(StatePatchNamePartialTest);}
