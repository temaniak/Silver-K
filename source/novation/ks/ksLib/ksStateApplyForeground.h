#pragma once
#include "ksStateApplyService.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateApplyForeground final {
public:
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}
 bool at(StateApplyService::Kind kind)const{return job_&&job_->service.kind()==kind;}
 bool at(StateApplyService::Component component)const{return job_&&job_->service.component()==component;}
 void resetMetrics(){initial=initialActive=active();started=completed=phases=0;components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0,initialActive=0;uint32_t components=0;std::array<uint64_t,7>kinds{};
private:
 struct Job{StateApplyService service;uint32_t caller,recipeDescriptor=0;int depth;uint8_t priority;Job(StateApplyService::Kind kind,StateApplyService::Input input,uint32_t sp,int nesting,uint8_t mask):service(kind,input),caller(sp),depth(nesting),priority(mask){}};
 std::optional<Job>job_;
};
}
