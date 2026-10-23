#pragma once
#include "ksStateEventsService.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
// Admission and final result publication for callers retaining the migration
// ABI. The complete owned event graph never materializes internal CPU frames.
class StateEventsForeground final {
public:
 bool step(ControllerCpu&);
 bool active()const{return job_.has_value();}
 bool at(StateEventsService::Component component)const{return job_&&job_->service.component()==component;}
 bool at(StateEventsService::Kind kind)const{return job_&&job_->service.kind()==kind;}
 void resetMetrics(){started=completed=phases=0;initial=active();components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,9>kinds{};
private:
 struct Job{
  StateEventsService service;uint32_t caller;int depth;
  Job(StateEventsService::Kind kind,StateEventsService::Input input,uint32_t stack,int level):service(kind,input),caller(stack),depth(level){}
 };
 std::optional<Job>job_;
};
}
