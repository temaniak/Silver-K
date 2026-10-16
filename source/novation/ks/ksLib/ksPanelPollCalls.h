#pragma once
#include "ksPanelPollTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class PanelPollCalls final{public:static bool candidate(unsigned p){return p>=0x8ea&&p<0x8fc;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,partialEntries=0;private:struct Call{PanelPollTask task;uint32_t token;explicit Call(uint32_t t):token(t){}};std::array<std::optional<Call>,16>calls{};};}

namespace ks{using PanelPollPartialTest=bool(*)(ControllerCpu&);void setPanelPollPartialForTests(PanelPollPartialTest);}
