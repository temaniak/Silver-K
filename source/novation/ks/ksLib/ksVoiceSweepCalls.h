#pragma once
#include "ksVoiceSweepTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class VoiceSweepCalls final {public:
 static bool candidate(unsigned p){return p>=0x9484&&p<=0x958c;}
 bool step(ControllerCpu&);void resetMetrics(){started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0;
private:struct Call {VoiceSweepTask task;uint32_t token;unsigned depth=0;Call(VoiceSweepTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};
};
using VoiceSweepPartialTest=bool(*)(ControllerCpu&);
void setVoiceSweepPartialForTests(VoiceSweepPartialTest);
}
