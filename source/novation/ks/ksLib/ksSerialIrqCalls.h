#pragma once
#include "ksSerialIrqTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class SerialIrqCalls final{public:static bool candidate(unsigned p){return(p>=0x105e&&p<0x10d0)||(p>=0x114e&&p<0x119a);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,partialEntries=0;private:struct Call{SerialIrqTask task;uint32_t token;unsigned depth=0;Call(SerialIrqTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using SerialIrqPartialTest=bool(*)(ControllerCpu&);void setSerialIrqPartialForTests(SerialIrqPartialTest);}
