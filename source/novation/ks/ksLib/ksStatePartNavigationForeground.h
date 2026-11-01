#pragma once
#include "ksStatePartNavigationService.h"
#include "ksStateFieldForeground.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StatePartNavigationForeground final {
public:
 using CallerAbi=StateFieldForeground::CallerAbi;
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}const StatePartNavigationService*service()const{return job_?&job_->service:nullptr;}
 bool at(StatePartNavigationService::Kind k)const{return service()&&service()->kind()==k;}bool at(StatePartNavigationService::Component c)const{return service()&&service()->component()==c;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 static void project(CallerAbi&,const StatePartNavigationService&);
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,5>kinds{};
private:
 struct Job{Job(StatePartNavigationService::Kind k,StatePartNavigationService::Input i,uint32_t c,int d,uint8_t p):service(k,i),caller(c),depth(d),priority(p){}StatePartNavigationService service;uint32_t caller=0;int depth=0;uint8_t priority=0;};std::optional<Job>job_;
};}
