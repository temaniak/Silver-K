#pragma once
#include "ksStateStoreServicesTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateStoreServicesCalls final {public:static bool candidate(unsigned p){return(p>=0x668c&&p<0x66dc)||p==0x6662||p==0x55b8;}void retireReturns(ControllerCpu&);bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateStoreServicesTask task;uint32_t token;Call(StateStoreServicesTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16> calls{};};using StateStoreServicesPartialTest=bool(*)(ControllerCpu&);void setStateStoreServicesPartialForTests(StateStoreServicesPartialTest);}
