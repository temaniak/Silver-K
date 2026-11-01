#pragma once
#include "ksVoicePopulationTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class VoicePopulationCalls final {
public:
 static bool candidate(unsigned p){return p>=0xa524&&p<=0xa5fa;}
 bool step(ControllerCpu&);
 void resetMetrics(){started=completed=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0;
private:
 struct Call {VoicePopulationTask task;uint32_t patch,token;Call(uint32_t p,uint32_t t):patch(p),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using VoicePopulationPartialTest=bool(*)(ControllerCpu&);
void setVoicePopulationPartialForTests(VoicePopulationPartialTest);
}
