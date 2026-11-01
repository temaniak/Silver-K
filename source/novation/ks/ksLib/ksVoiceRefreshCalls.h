#pragma once
#include "ksVoiceRefreshTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class VoiceRefreshCalls final {public:static bool candidate(unsigned p){return p>=0x958e&&p<=0x96d4;}bool step(ControllerCpu&);void resetMetrics(){started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0;
private:struct Call{VoiceRefreshTask task;uint32_t token;unsigned depth=0;Call(VoiceRefreshTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};
};}

namespace ks {using VoiceRefreshPartialTest=bool(*)(ControllerCpu&);void setVoiceRefreshPartialForTests(VoiceRefreshPartialTest); }
