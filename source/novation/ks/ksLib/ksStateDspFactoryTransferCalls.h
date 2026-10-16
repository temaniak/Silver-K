#pragma once
#include "ksStateDspFactoryTransferTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateDspFactoryTransferCalls final {public:static bool candidate(unsigned p){return p>=0x77a0&&p<0x77dc;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateDspFactoryTransferTask task;uint32_t token,source;Call(StateDspFactoryTransferTask::Kind kind,uint32_t descriptor,uint16_t count,uint32_t t,uint32_t s):task(kind,descriptor,count),token(t),source(s){}};std::array<std::optional<Call>,16> calls{};};using StateDspFactoryTransferPartialTest=bool(*)(ControllerCpu&);void setStateDspFactoryTransferPartialForTests(StateDspFactoryTransferPartialTest);}
