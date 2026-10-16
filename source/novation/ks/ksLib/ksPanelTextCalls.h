#pragma once
#include "ksPanelTextTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class PanelTextCalls final{public:static bool candidate(unsigned p){return p>=0x976&&p<0x9dc;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,partialEntries=0;private:struct Call{PanelTextTask task;uint32_t token;unsigned depth=0;Call(PanelTextTask::Kind k,uint32_t t):task(k),token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using PanelTextPartialTest=bool(*)(ControllerCpu&);void setPanelTextPartialForTests(PanelTextPartialTest);}
