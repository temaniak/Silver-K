#pragma once
#include "ksStatePlaybackModeService.h"
#include "ksStatePlaybackModeProjection.h"
#include "ksStateProgramLoadForeground.h"
#include "ksStateFieldForeground.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StatePlaybackModeForeground final {
public:
 using CallerAbi=StateFieldForeground::CallerAbi;
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}const StatePlaybackModeService*service()const{return job_?&job_->service:nullptr;}
 bool at(StatePlaybackModeService::Kind k)const{return service()&&service()->kind()==k;}bool at(StatePlaybackModeService::Component c)const{return service()&&service()->component()==c;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,10>kinds{};
 static void project(CallerAbi&,const StatePlaybackModeService&,uint32_t recipe=0);
 template<class A>static void projectLower(A&a,const StatePlaybackModeCoordinator&s){playbackModeProjection::lower(a,s);}
private:
 struct Job{Job(StatePlaybackModeService::Kind k,StatePlaybackModeService::Input i,uint32_t c,int d,uint8_t p):service(k,i),caller(c),depth(d),priority(p){}StatePlaybackModeService service;uint32_t caller=0,recipe=0;int depth=0;uint8_t priority=0;};std::optional<Job>job_;
};}
