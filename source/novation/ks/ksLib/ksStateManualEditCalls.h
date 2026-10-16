#pragma once
#include "ksStateManualEditTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateManualEditCalls final {public:static bool candidate(unsigned p){return p>=0x6ae6&&p<=0x6bf0;}void retireReturns(ControllerCpu&);bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateManualEditTask task;uint32_t token,live=0,writeBase=0;Call(uint32_t reference,uint8_t value,uint32_t t):task(reference,value),token(t){}};std::array<std::optional<Call>,16> calls{};};using StateManualEditPartialTest=bool(*)(ControllerCpu&);void setStateManualEditPartialForTests(StateManualEditPartialTest);}
