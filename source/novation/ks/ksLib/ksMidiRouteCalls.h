#pragma once
#include "ksMidiRouteTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class MidiRouteCalls final {
public:
 static bool candidate(unsigned p){return p>=0x328e&&p<0x33b8;}
 bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:
 struct Call {MidiRouteTask task;uint32_t token,handlerReference;unsigned depth=0;Call(uint32_t t,uint32_t reference):token(t),handlerReference(reference){}};
 std::array<std::optional<Call>,16> calls{};
};
using MidiRoutePartialTest=bool(*)(ControllerCpu&);void setMidiRoutePartialForTests(MidiRoutePartialTest);
}
