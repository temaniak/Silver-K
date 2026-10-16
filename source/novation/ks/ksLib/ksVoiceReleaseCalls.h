#pragma once
#include "ksVoiceReleaseTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class VoiceReleaseCalls final {
public:
 static bool candidate(unsigned p){return p>=0x92c0&&p<=0x9482;}
 bool step(ControllerCpu&);
 void resetMetrics(){started=completed=partialEntries=restarted=abandoned=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,partialEntries=0,restarted=0,abandoned=0;
private:
 struct Call {VoiceReleaseTask task;uint32_t token,patch=0;unsigned depth=0;
  Call(VoiceReleaseTask::Mode m,uint8_t note,uint16_t cut,uint32_t t):task(m,note,cut),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using VoiceReleasePartialTest=bool(*)(ControllerCpu&);
void setVoiceReleasePartialForTests(VoiceReleasePartialTest);
}
