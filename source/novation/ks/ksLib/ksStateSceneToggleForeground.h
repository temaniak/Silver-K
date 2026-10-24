#pragma once
#include "ksStateSceneToggleService.h"
#include "ksStateFieldForeground.h"
#include "ksStateSceneForeground.h"
#include "ksStateSceneEditForeground.h"
#include "ksStateSaveForeground.h"
namespace ks {class ControllerCpu;
class StateSceneToggleForeground final {
public:
 using CallerAbi=StateFieldForeground::CallerAbi;using Kind=StateSceneToggleService::Kind;using Component=StateSceneToggleService::Component;
 bool step(ControllerCpu&);unsigned active()const{return job_?1:0;}const StateSceneToggleService*service()const{return job_?&job_->service:nullptr;}
 const StateFieldService*field()const{return service()?service()->field():nullptr;}
 bool at(Kind k)const{return job_&&job_->service.kind()==k;}bool at(Component c)const{return job_&&job_->service.component()==c;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;std::array<uint64_t,2>kinds{};
 static void project(CallerAbi&,const StateSceneToggleService&);
private:
 struct Job{Job(Kind k,StateSceneToggleService::Input in,uint32_t c,int d,uint8_t p):service(k,in),caller(c),depth(d),priority(p){}StateSceneToggleService service;uint32_t caller;int depth;uint8_t priority,writePriority=0;};
 std::optional<Job>job_;
};}
