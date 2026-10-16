#pragma once
#include "ksVoiceSelectionTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class VoiceSelectionCalls final {
public:
 static bool candidate(unsigned p){return (p>=0xa338&&p<0xa524)||(p>=0x8fc0&&p<=0x8fca);}
 bool step(ControllerCpu&);
 void resetMetrics(){started=completed=gateStarted=gateCompleted=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();for(auto& c:gates)c.reset();}
 uint64_t started=0,completed=0,gateStarted=0,gateCompleted=0,partialEntries=0;
private:
 struct Call {VoiceSelectionTask task;uint32_t patch,token;Call(uint32_t p,uint32_t t):patch(p),token(t){}};
 struct Gate {VoiceGateOffTask task;uint32_t token;explicit Gate(uint32_t t):token(t){}};
 std::array<std::optional<Call>,16> calls{};std::array<std::optional<Gate>,16> gates{};
};
using VoiceSelectionPartialTest=bool(*)(ControllerCpu&);
void setVoiceSelectionPartialForTests(VoiceSelectionPartialTest);
}
