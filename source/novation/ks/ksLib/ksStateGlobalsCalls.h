#pragma once
#include "ksStateGlobalsTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateGlobalsCalls final {public:static bool candidate(unsigned p){return(p>=0x69c6&&p<0x69e2)||(p>=0x6a68&&p<0x6ae6);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateGlobalsTask task;uint32_t token,live=0;Call(StateGlobalsTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16> calls{};};using StateGlobalsPartialTest=bool(*)(ControllerCpu&);void setStateGlobalsPartialForTests(StateGlobalsPartialTest);}
