#pragma once
#include "ksMidiAuxTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class MidiAuxCalls final {
public:
 static bool candidate(unsigned p){return p>=0x358a&&p<0x3642;}
 bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t initialActive=0;uint64_t started=0,completed=0,handedOff=0,partialEntries=0;
private:
 struct Call {MidiAuxTask task;uint32_t token;Call(MidiAuxTask::Kind k,uint32_t t):task(k),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using MidiAuxPartialTest=bool(*)(ControllerCpu&);void setMidiAuxPartialForTests(MidiAuxPartialTest);
}
