#pragma once
#include "ksPartFeedbackService.h"
#include <optional>
namespace ks {class ControllerCpu;
class PartFeedbackForeground final {
public:
 bool step(ControllerCpu&);bool active()const{return job_.has_value();}
 const PartFeedbackService*service()const{return job_?&job_->service:nullptr;}
 template<class T>const T*task()const{return job_?job_->service.template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=0;components=0;}
 uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;
private:
 struct Job{PartFeedbackService service;uint32_t caller;int depth;uint8_t priority;Job(uint32_t sp,int d,uint8_t exr):caller(sp),depth(d),priority(exr){}};std::optional<Job>job_;
};}
