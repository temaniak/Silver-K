#pragma once
#include "ksArpScheduleTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class ArpScheduleCalls final {
public:static bool candidate(unsigned p){return p>=0x370e&&p<=0x376e;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,partialEntries=0;
private:struct Call {ArpScheduleTask task;uint32_t token;unsigned depth=0;explicit Call(uint32_t t):token(t){}};std::array<std::optional<Call>,16> calls{};};using ArpSchedulePartialTest=bool(*)(ControllerCpu&);void setArpSchedulePartialForTests(ArpSchedulePartialTest);}
