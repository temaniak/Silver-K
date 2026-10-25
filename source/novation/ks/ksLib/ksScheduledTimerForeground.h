#pragma once
#include "ksScheduledTimerService.h"
#include <array>
namespace ks {class ControllerCpu;
class ScheduledTimerForeground final {
public:
 bool step(ControllerCpu&);unsigned active()const{return job_?1:0;}const ScheduledTimerService*service()const{return job_?&job_->service:nullptr;}
 bool at(ScheduledTimerService::Kind k,unsigned slot)const{return job_&&job_->service.kind()==k&&job_->service.slot()==slot;}bool at(ScheduledTimerService::Component c)const{return job_&&job_->service.component()==c;}
 template<class T>const T*task()const{return job_?job_->service.template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);returnedCallbacks.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,15>kinds{};std::array<uint64_t,10>returnedCallbacks{};
private:
 struct Job{Job(ScheduledTimerService::Kind k,unsigned slot,ScheduledTimerService::Input in,uint32_t sp,int d,uint8_t p):service(k,slot,in),caller(sp),depth(d),priority(p){}ScheduledTimerService service;uint32_t caller=0;int depth=0;uint8_t priority=0;};std::optional<Job>job_;
};}
