#pragma once
#include "ksPanelPedalOutputService.h"
#include <array>
namespace ks {
class ControllerCpu;
class PanelPedalOutputForeground final {
public:
 bool step(ControllerCpu&);bool active()const{return job_.has_value();}
 template<class T>const T*task()const{return job_?job_->service.template task<T>():nullptr;}
 const PanelPedalOutputService*service()const{return job_?&job_->service:nullptr;}
 bool at(PanelPedalOutputService::Kind k)const{return job_&&job_->service.kind()==k;}bool at(PanelPedalOutputService::Component c)const{return job_&&job_->service.component()==c;}
 void resetMetrics(){started=completed=phases=0;initial=active();components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,2>kinds{};
private:
 struct Job{PanelPedalOutputService service;uint32_t caller;int depth;uint8_t priority;Job(PanelPedalOutputService::Kind k,uint16_t event,uint32_t sp,int level,uint8_t exr):service(k,{event}),caller(sp),depth(level),priority(exr){}};
 std::optional<Job>job_;
};
}
