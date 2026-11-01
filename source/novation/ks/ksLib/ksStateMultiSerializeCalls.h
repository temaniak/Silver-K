#pragma once
#include "ksStateMultiSerializeTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateMultiSerializeCalls final {public:static bool candidate(unsigned p){return p>=0x71ba&&p<0x72e0;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateMultiSerializeTask task;uint32_t token,original=0,partSource=0;explicit Call(uint32_t t):token(t){}};std::array<std::optional<Call>,16> calls{};};using StateMultiSerializePartialTest=bool(*)(ControllerCpu&);void setStateMultiSerializePartialForTests(StateMultiSerializePartialTest);}
