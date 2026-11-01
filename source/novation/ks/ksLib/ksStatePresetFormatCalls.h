#pragma once
#include "ksStatePresetFormatTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StatePresetFormatCalls final {public:static bool candidate(unsigned p){return(p>=0x64e4&&p<0x6662)||p==0x668c;}void retireReturns(ControllerCpu&);bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StatePresetFormatTask task;uint32_t token,program,input;Call(StatePresetFormatTask::Entry e,uint32_t t,uint32_t p,uint32_t i):task(e),token(t),program(p),input(i){}};std::array<std::optional<Call>,16> calls{};};using StatePresetFormatPartialTest=bool(*)(ControllerCpu&);void setStatePresetFormatPartialForTests(StatePresetFormatPartialTest);}
