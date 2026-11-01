#pragma once
#include "ksArpMainTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class ArpMainCalls final {public:static bool candidate(unsigned p){return p>=0x3770&&p<=0x3a36;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:struct Call {ArpMainTask task;uint32_t token;unsigned depth=0;explicit Call(uint32_t t):token(t){}};std::array<std::optional<Call>,16> calls{};};using ArpMainPartialTest=bool(*)(ControllerCpu&);void setArpMainPartialForTests(ArpMainPartialTest);}
