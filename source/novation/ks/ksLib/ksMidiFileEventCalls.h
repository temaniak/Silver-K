#pragma once
#include "ksMidiFileEventTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class MidiFileEventCalls final{public:static bool candidate(unsigned p){return(p>=0xb2a2&&p<0xb2e0)||(p>=0xb34a&&p<0xb3c0);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:struct Call{MidiFileEventTask task;uint32_t token;unsigned depth=0;Call(MidiFileEventTask::Kind kind,uint32_t stream,uint8_t byte,uint32_t t):task(kind,stream,byte),token(t){}};std::array<std::optional<Call>,16>calls{};};using MidiFileEventPartialTest=bool(*)(ControllerCpu&);void setMidiFileEventPartialForTests(MidiFileEventPartialTest);}
