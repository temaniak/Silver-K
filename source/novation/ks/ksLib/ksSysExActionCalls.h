#pragma once
#include "ksSysExActionTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class SysExActionCalls final{public:static bool candidate(unsigned p){return p>=0xab42&&p<0xacaa;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:struct Call{SysExActionTask task;uint32_t token;unsigned depth=0;Call(SysExActionTask::Kind k,uint32_t record,uint32_t source,uint16_t count,uint8_t program,uint32_t t):task(k,record,source,count,program),token(t){}};std::array<std::optional<Call>,16>calls{};};using SysExActionPartialTest=bool(*)(ControllerCpu&);void setSysExActionPartialForTests(SysExActionPartialTest);}
