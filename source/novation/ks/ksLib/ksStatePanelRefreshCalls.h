#pragma once
#include "ksStatePanelRefreshTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StatePanelRefreshCalls final {public:static bool candidate(unsigned p){return p>=0x6c3c&&p<0x6cfe;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StatePanelRefreshTask task;uint32_t token,current=0;explicit Call(uint32_t t):token(t){}};std::array<std::optional<Call>,16> calls{};};using StatePanelRefreshPartialTest=bool(*)(ControllerCpu&);void setStatePanelRefreshPartialForTests(StatePanelRefreshPartialTest);}
