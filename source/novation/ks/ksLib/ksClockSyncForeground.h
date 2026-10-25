#pragma once
#include "ksClockSyncService.h"
#include "ksStateSaveAbi.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class ClockSyncForeground final {
public:
 using CallerAbi=StateSaveCallerAbi;
 bool step(ControllerCpu&);bool active()const{return bool(job_);}
 const ClockSyncService*service()const{return job_?&job_->work:nullptr;}
 bool at(ClockSyncService::Component c)const{return job_&&job_->work.component()==c;}
 template<class T>const T*task()const{return job_?job_->work.template task<T>():nullptr;}
 void resetMetrics(){started=completed=phases=components=0;initial=active();kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,1>kinds{};
 static void project(CallerAbi&,const ClockSyncService&);
private:
 struct Job{Job(uint32_t p,int d):caller(p),depth(d){}ClockSyncService work{};uint32_t caller;int depth;};std::optional<Job>job_;
};}
