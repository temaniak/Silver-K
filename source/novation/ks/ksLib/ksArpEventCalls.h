#pragma once
#include "ksArpEventTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class ArpEventCalls final {public:static bool candidate(unsigned p){return (p>=0x3ab2&&p<0x3ca4)||(p>=0x3f18&&p<0x4000);}bool step(ControllerCpu&);void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)n+=bool(c);return n;}uint64_t initialActive=0,started=0,completed=0,partialEntries=0;
private:struct Call{ArpEventTask task;uint32_t token;unsigned depth=0;Call(ArpEventTask::Kind kind,uint8_t part,uint8_t note,uint8_t velocity,uint32_t t):task(kind,part,note,velocity),token(t){}};std::array<std::optional<Call>,16> calls{};};using ArpEventPartialTest=bool(*)(ControllerCpu&);void setArpEventPartialForTests(ArpEventPartialTest);}
