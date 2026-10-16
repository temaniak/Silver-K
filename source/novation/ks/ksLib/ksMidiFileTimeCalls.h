#pragma once
#include "ksMidiFileTimeTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class MidiFileTimeCalls final{public:static bool candidate(unsigned p){return(p>=0xb2e0&&p<0xb34a)||(p>=0xb3c0&&p<0xb3ec)||(p>=0xb494&&p<0xb4b8);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,partialEntries=0;
private:struct Call{MidiFileTimeTask task;uint32_t token;Call(MidiFileTimeTask::Kind k,uint32_t stream,uint32_t tempo,uint32_t record,uint32_t t):task(k,stream,tempo,record),token(t){}};std::array<std::optional<Call>,16>calls{};};using MidiFileTimePartialTest=bool(*)(ControllerCpu&);void setMidiFileTimePartialForTests(MidiFileTimePartialTest);}
