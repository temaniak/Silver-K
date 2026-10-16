#pragma once
#include "ksStateParameterExpandTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateParameterExpandCalls final {public:static bool candidate(unsigned p){return p>=0x6726&&p<0x687e;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateParameterExpandTask task;uint32_t token,source,target;Call(StateParameterExpandTask::Entry e,uint32_t t,uint32_t s,uint32_t d):task(e),token(t),source(s),target(d){}};std::array<std::optional<Call>,16> calls{};};using StateParameterExpandPartialTest=bool(*)(ControllerCpu&);void setStateParameterExpandPartialForTests(StateParameterExpandPartialTest);}
