#pragma once
#include "ksInternalKeyOutputService.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class InternalKeyOutputForeground final {
public:
 struct CallerAbi{std::array<uint32_t,7>values{};uint8_t flags=0;};
 bool step(ControllerCpu&);static void project(CallerAbi&,const InternalKeyOutputService&);
 template<class A>static void project(A&a,const InternalKeyOutputService&s){CallerAbi b;b.values=a.values;b.flags=a.flags;project(b,s);a.values=b.values;a.flags=b.flags;}
 unsigned active()const{return job_?1:0;}const InternalKeyOutputService*service()const{return job_?&job_->service:nullptr;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;std::array<uint64_t,4>kinds{};
private:
 struct Job{Job(InternalKeyOutputService::Kind k,InternalKeyOutputService::Input i,uint32_t c,int d,uint8_t p):service(k,i),caller(c),depth(d),priority(p){}InternalKeyOutputService service;uint32_t caller;int depth;uint8_t priority;};
 std::optional<Job>job_;
};
}
