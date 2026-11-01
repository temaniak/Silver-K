#pragma once
#include "ksStateNameEditorTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateNameEditorCalls final {public:static bool candidate(unsigned p){return(p>=0x752c&&p<0x75e8)||(p>=0x75fa&&p<0x760e)||p==0x5340;}bool step(ControllerCpu&);bool retireReturns(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateNameEditorTask task;uint32_t token,name;Call(StateNameEditorTask::Kind k,uint8_t delta,uint32_t t,uint32_t n):task(k,delta),token(t),name(n){}};std::array<std::optional<Call>,16> calls{};};using StateNameEditorPartialTest=bool(*)(ControllerCpu&);void setStateNameEditorPartialForTests(StateNameEditorPartialTest);}
