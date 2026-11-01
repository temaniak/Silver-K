#pragma once
#include "ksPanelControlsService.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class PanelControlsForeground final {
public:
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}
 bool at(PanelControlsService::Component component)const{return job_&&job_->service.component()==component;}
 void resetMetrics(){initialActive=active();started=completed=phases=0;components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initialActive=0;uint32_t components=0;std::array<uint64_t,7> kinds{};
private:
 struct Job {PanelControlsService service;uint32_t caller;int depth;unsigned phase=0;Job(PanelControlsService::Kind kind,uint32_t stack,int level):service(kind),caller(stack),depth(level){}};
 std::optional<Job> job_;
};
}
