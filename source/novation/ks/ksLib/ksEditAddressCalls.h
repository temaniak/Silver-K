#pragma once
#include "ksEditAddressTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class EditAddressCalls final{public:static bool candidate(unsigned p){return p>=0xb660&&p<0xb716;}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=handedOff=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,handedOff=0,partialEntries=0;
private:struct Call{EditAddressTask task;uint32_t token;Call(EditAddressTask::Kind kind,uint8_t delta,uint32_t t):task(kind,delta),token(t){}};std::array<std::optional<Call>,16>calls{};};using EditAddressPartialTest=bool(*)(ControllerCpu&);void setEditAddressPartialForTests(EditAddressPartialTest);}
