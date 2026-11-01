#pragma once
#include "ksLegacySysExTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class LegacySysExCalls final{public:static bool candidate(unsigned p){return p>=0xaf94&&p<0xb06c;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:struct Call{LegacySysExTask task;uint32_t token;Call(LegacySysExTask::Kind k,uint32_t record,uint32_t t):task(k,record),token(t){}};std::array<std::optional<Call>,16>calls{};};using LegacySysExPartialTest=bool(*)(ControllerCpu&);void setLegacySysExPartialForTests(LegacySysExPartialTest);}
