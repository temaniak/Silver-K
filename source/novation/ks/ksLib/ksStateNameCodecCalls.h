#pragma once
#include "ksStateNameCodecTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateNameCodecCalls final {public:static bool candidate(unsigned p){return(p>=0x75e8&&p<0x75fa)||(p>=0x760e&&p<0x7666);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateNameCodecTask task;uint32_t token;Call(StateNameCodecTask::Kind k,uint8_t character,uint32_t t):task(k,character),token(t){}};std::array<std::optional<Call>,16> calls{};};using StateNameCodecPartialTest=bool(*)(ControllerCpu&);void setStateNameCodecPartialForTests(StateNameCodecPartialTest);}
