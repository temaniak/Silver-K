#pragma once
#include "ksStateMultiSaveTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateMultiSaveCalls final {public:static bool candidate(unsigned p){return(p>=0x71a6&&p<0x71ba)||(p>=0x72e0&&p<0x7354)||p==0x66d2;}bool step(ControllerCpu&);bool retireReturns(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateMultiSaveTask task;uint32_t token,program,source;Call(StateMultiSaveTask::Entry e,uint32_t t,uint32_t p,uint32_t s):task(e),token(t),program(p),source(s){}};std::array<std::optional<Call>,16> calls{};};using StateMultiSavePartialTest=bool(*)(ControllerCpu&);void setStateMultiSavePartialForTests(StateMultiSavePartialTest);}
