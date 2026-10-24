#pragma once
#include "ksStateVoiceRefreshService.h"
#include "ksGlobalUpdateForeground.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StateVoiceRefreshForeground final {
public:
 using CallerAbi=GlobalUpdateForeground::CallerAbi;
 bool step(ControllerCpu&);
 static void project(CallerAbi&a,const StateVoiceRefreshService&s){if(!s.complete()||!s.global()||!s.summary().voiceObserved)throw std::logic_error("Incomplete full voice projection");a.values[3]=s.summary().lastVoiceReference;GlobalUpdateForeground::project(a,*s.global());}
 unsigned active()const{return job_?1:0;}const StateVoiceRefreshService*service()const{return job_?&job_->service:nullptr;}
 bool at(StateVoiceRefreshService::Component c)const{return service()&&service()->component()==c;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,1>kinds{};
private:
 struct Job{Job(uint32_t c,int d,uint8_t p):caller(c),depth(d),priority(p){}StateVoiceRefreshService service;uint32_t caller;int depth;uint8_t priority;};std::optional<Job>job_;
};}
