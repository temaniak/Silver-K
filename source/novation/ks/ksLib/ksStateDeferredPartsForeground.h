#pragma once
#include "ksStateDeferredPartsAbi.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StateDeferredPartsForeground final {
public:
 using CallerAbi=StateVoiceRefreshForeground::CallerAbi;bool step(ControllerCpu&);
 static void project(CallerAbi&a,const StateDeferredPartsService&s){projectStateDeferredParts(a,s);}
 unsigned active()const{return job_?1u:0u;}const StateDeferredPartsService*service()const{return job_?&job_->service:nullptr;}
 bool at(StateDeferredPartsService::Component c)const{return service()&&service()->component()==c;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=processed=voiceUploads=partMask=0;kinds.fill(0);}uint64_t started=0,completed=0,phases=0,initial=0;uint64_t processed=0,voiceUploads=0;uint32_t components=0,partMask=0;std::array<uint64_t,1>kinds{};
private:
 struct Job{Job(uint32_t c,int d,uint8_t p):caller(c),depth(d),priority(p){}StateDeferredPartsService service;uint32_t caller;int depth;uint8_t priority;};std::optional<Job>job_;
};}
