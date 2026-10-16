#pragma once
#include "ksServiceCycleTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class ServiceCycleCalls final {
public:
 static bool candidate(unsigned p){return p>=0x4338&&p<=0x4378;}
 bool step(ControllerCpu&);void resetMetrics(){started=cyclesCompleted=partialEntries=0;}
 void discardCalls(){for(auto& c:calls)c.reset();}
 uint64_t started=0,cyclesCompleted=0,partialEntries=0;
private:struct Call {ServiceCycleTask task;uint32_t token;explicit Call(uint32_t t):token(t){}};std::array<std::optional<Call>,16>calls{};
};
using ServiceCyclePartialTest=bool(*)(ControllerCpu&);
void setServiceCyclePartialForTests(ServiceCyclePartialTest);
}
