#pragma once
#include "ksSysExDumpTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class SysExDumpCalls final{public:static bool candidate(unsigned p){return p>=0xacaa&&p<0xaf94;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:struct Call{SysExDumpTask task;uint32_t token;unsigned depth=0;Call(SysExDumpTask::Kind k,uint16_t selection,uint8_t slot,uint32_t t):task(k,selection,slot),token(t){}};std::array<std::optional<Call>,16>calls{};};using SysExDumpPartialTest=bool(*)(ControllerCpu&);void setSysExDumpPartialForTests(SysExDumpPartialTest);}
