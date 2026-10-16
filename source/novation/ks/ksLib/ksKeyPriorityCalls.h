#pragma once
#include "ksKeyPriorityTask.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class KeyPriorityCalls final {
public:
 static bool candidate(unsigned p){return (p>=0x3024&&p<=0x3072)||(p>=0x3118&&p<=0x3178);}
 bool step(ControllerCpu&);
 void resetMetrics(){compactStarted=compactCompleted=monoStarted=monoCompleted=partialEntries=0;}
 void discardCalls(){for(auto& c:compacts)c.reset();for(auto& c:monos)c.reset();}
 uint64_t compactStarted=0,compactCompleted=0,monoStarted=0,monoCompleted=0,partialEntries=0;
private:
 template<class Task>struct Call {Task task;uint32_t token;unsigned depth=0;explicit Call(uint32_t t):token(t){}};
 std::array<std::optional<Call<KeyCompactTask>>,16> compacts{};
 std::array<std::optional<Call<MonoPriorityTask>>,16> monos{};
};
using KeyPriorityPartialTest=bool(*)(ControllerCpu&);
void setKeyPriorityPartialForTests(KeyPriorityPartialTest);
}
