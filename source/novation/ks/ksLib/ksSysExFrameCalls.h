#pragma once
#include "ksSysExFrameTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class SysExFrameCalls final{public:static bool candidate(unsigned p){return p>=0xa924&&p<=0xab40;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:struct Call{SysExFrameTask task;uint32_t token;Call(SysExFrameTask::Kind k,uint8_t data,uint32_t payload,uint32_t t):task(k,data,payload),token(t){}};std::array<std::optional<Call>,16>calls{};};using SysExFramePartialTest=bool(*)(ControllerCpu&);void setSysExFramePartialForTests(SysExFramePartialTest);}
