#pragma once
#include "ksInternalNoteTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class InternalNoteCalls final{public:static bool candidate(unsigned p){return p>=0xa882&&p<=0xa922;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:struct Call{InternalNoteTask task;uint32_t token;unsigned depth=0;Call(InternalNoteTask::Kind k,uint8_t part,uint8_t note,uint32_t payload,uint32_t t):task(k,part,note,payload),token(t){}};std::array<std::optional<Call>,16>calls{};};using InternalNotePartialTest=bool(*)(ControllerCpu&);void setInternalNotePartialForTests(InternalNotePartialTest);}
