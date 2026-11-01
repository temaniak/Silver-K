#pragma once
#include "ksStateBankDisplayTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateBankDisplayCalls final {public:static bool candidate(unsigned p){return(p>=0x7354&&p<0x7364)||(p>=0x73e2&&p<0x74d4)||p==0x9662||p==0x9b8||p==0x976;}bool step(ControllerCpu&);bool retireReturns(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateBankDisplayTask task;uint32_t token,table=0;Call(StateBankDisplayTask::Entry e,uint32_t t):task(e),token(t){}};std::array<std::optional<Call>,16> calls{};};using StateBankDisplayPartialTest=bool(*)(ControllerCpu&);void setStateBankDisplayPartialForTests(StateBankDisplayPartialTest);}
