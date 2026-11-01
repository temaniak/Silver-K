#pragma once
#include "ksArpSelectTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class ArpSelectCalls final {public:static bool candidate(unsigned p){return p>=0x3ca4&&p<0x3f18;}bool step(ControllerCpu&);void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)n+=bool(c);return n;}uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:struct Call{ArpSelectTask task;uint32_t token;unsigned depth=0;Call(ArpSelectTask::Kind k,uint8_t part,uint8_t mode,uint8_t count,uint8_t last,uint8_t candidate,uint32_t t):task(k,part,mode,count,last,candidate),token(t){}};std::array<std::optional<Call>,16> calls{};};using ArpSelectPartialTest=bool(*)(ControllerCpu&);void setArpSelectPartialForTests(ArpSelectPartialTest);}
