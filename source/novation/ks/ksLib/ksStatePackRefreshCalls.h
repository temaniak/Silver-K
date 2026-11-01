#pragma once
#include "ksStatePackRefreshTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StatePackRefreshCalls final {public:static bool candidate(unsigned p){return p>=0x687e&&p<0x68b0;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StatePackRefreshTask task;uint32_t token,source=0,recipe=0;Call(StatePackRefreshTask::Entry e,uint8_t g,uint32_t t):task(e,g),token(t){}};std::array<std::optional<Call>,16> calls{};};using StatePackRefreshPartialTest=bool(*)(ControllerCpu&);void setStatePackRefreshPartialForTests(StatePackRefreshPartialTest);}
