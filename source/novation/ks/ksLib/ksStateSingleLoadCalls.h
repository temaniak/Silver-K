#pragma once
#include "ksStateSingleLoadTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateSingleLoadCalls final {public:static bool candidate(unsigned p){return p>=0x63fe&&p<0x6494;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateSingleLoadTask task;uint32_t token,source,target=0,overlay=0,overlayTarget=0,lateTarget=0;Call(uint32_t t,uint32_t s):token(t),source(s){}};std::array<std::optional<Call>,16> calls{};};using StateSingleLoadPartialTest=bool(*)(ControllerCpu&);void setStateSingleLoadPartialForTests(StateSingleLoadPartialTest);}
