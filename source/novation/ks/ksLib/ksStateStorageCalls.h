#pragma once
#include "ksStateStorageTask.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;class StateStorageCalls final {public:static bool candidate(unsigned p){return(p>=0x629e&&p<0x62b2)||(p>=0x6fcc&&p<0x6ff4)||(p>=0x7190&&p<0x71a6)||(p>=0x7364&&p<0x737a)||(p>=0x74d4&&p<0x752c);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto& c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto& c:calls)c.reset();}uint64_t started=0,completed=0,partialEntries=0,initialActive=0;
private:struct Call{StateStorageTask task;uint32_t token;Call(StateStorageTask::Kind k,uint8_t p,uint32_t t):task(k,p),token(t){}};std::array<std::optional<Call>,16> calls{};};using StateStoragePartialTest=bool(*)(ControllerCpu&);void setStateStoragePartialForTests(StateStoragePartialTest);}
