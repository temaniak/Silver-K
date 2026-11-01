#pragma once
#include "ksStatePackFieldsTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StatePackFieldsCalls final {public:static bool candidate(unsigned p){return p>=0x68b0&&p<0x69c6;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StatePackFieldsTask task;uint32_t token,source;Call(StatePackFieldsTask::Kind k,uint32_t t,uint32_t s):task(k),token(t),source(s){}};std::array<std::optional<Call>,16> calls{};};using StatePackFieldsPartialTest=bool(*)(ControllerCpu&);void setStatePackFieldsPartialForTests(StatePackFieldsPartialTest);}
