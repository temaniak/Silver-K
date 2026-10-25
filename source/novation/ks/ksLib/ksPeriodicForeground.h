#pragma once
#include "ksPeriodicService.h"
#include <optional>
#include <array>
namespace ks {class ControllerCpu;
class PeriodicForeground final {
public:
 bool step(ControllerCpu&);bool active()const{return job_.has_value();}
 const PeriodicService*service()const{return job_?&job_->service:nullptr;}
 template<class T>const T*task()const{return job_?job_->service.template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=0;kinds.fill(0);}
 uint64_t initial=0,started=0,completed=0,phases=0;std::array<uint64_t,4>kinds{};
private:
 struct Job{PeriodicService service;uint32_t caller;int depth;uint8_t priority;Job(PeriodicService::Kind k,uint32_t sp,int d,uint8_t exr):service(k),caller(sp),depth(d),priority(exr){}};
 std::optional<Job>job_;
};}
