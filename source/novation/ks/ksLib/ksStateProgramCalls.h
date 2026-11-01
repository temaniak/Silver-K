#pragma once
#include "ksStateProgramTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateProgramCalls final {public:static bool candidate(unsigned p){return(p>=0x6198&&p<0x61ec)||(p>=0x628c&&p<0x629e);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateProgramTask task;uint32_t token,source;Call(StateProgramTask::Kind k,uint16_t index,uint32_t t,uint32_t s):task(k,index),token(t),source(s){}};std::array<std::optional<Call>,16> calls{};};using StateProgramPartialTest=bool(*)(ControllerCpu&);void setStateProgramPartialForTests(StateProgramPartialTest);}
