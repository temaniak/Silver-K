#pragma once
#include "ksEditSceneTask.h"
#include <array>
#include <optional>
namespace ks{class ControllerCpu;class EditSceneCalls final{public:static bool candidate(unsigned p){return(p>=0xb786&&p<0xb83c)||(p>=0xb8ac&&p<0xb8e8);}bool step(ControllerCpu&);unsigned activeCalls()const{unsigned n=0;for(const auto&c:calls)if(c)++n;return n;}void resetMetrics(){initialActive=activeCalls();started=completed=partialEntries=0;}void discardCalls(){for(auto&c:calls)c.reset();}uint64_t initialActive=0,started=0,completed=0,partialEntries=0;
private:struct Call{EditSceneTask task;uint32_t token;unsigned depth=0;Call(EditSceneTask::Kind kind,bool save,uint32_t t):task(kind,save),token(t){}};std::array<std::optional<Call>,16>calls{};};using EditScenePartialTest=bool(*)(ControllerCpu&);void setEditScenePartialForTests(EditScenePartialTest);}
