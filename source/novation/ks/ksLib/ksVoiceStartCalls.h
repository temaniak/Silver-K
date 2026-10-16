#pragma once
#include "ksVoiceStartTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class VoiceStartCalls final {
public:
 static bool candidate(unsigned p){return (p>=0x902a&&p<=0x9082)||(p>=0x927e&&p<=0x92be);}
 bool step(ControllerCpu&);
 unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}
 void resetMetrics(){initialActive=activeCalls();started=completed=handoffs=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,completed=0,handoffs=0,partialEntries=0,initialActive=0;
private:
 struct Call {VoiceStartTask task;uint32_t token;Call(VoiceStartTask::Kind k,uint32_t t):task(k),token(t){}};
 std::array<std::optional<Call>,16> calls{};
};
using VoiceStartPartialTest=bool(*)(ControllerCpu&);
void setVoiceStartPartialForTests(VoiceStartPartialTest);
}
