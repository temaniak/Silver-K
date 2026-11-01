#pragma once
#include "ksEditDecisionTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class EditDecisionCalls final{public:static bool candidate(unsigned p){return(p>=0xb554&&p<0xb660)||(p>=0xb716&&p<0xb786)||(p>=0xb83c&&p<0xb8ac);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:struct Call{EditDecisionTask task;uint32_t token;Call(EditDecisionTask::Kind kind,uint8_t knob,uint8_t delta,uint8_t bank,uint32_t t):task(kind,knob,delta,bank),token(t){}};std::array<std::optional<Call>,16>calls{};};using EditDecisionPartialTest=bool(*)(ControllerCpu&);void setEditDecisionPartialForTests(EditDecisionPartialTest);}
