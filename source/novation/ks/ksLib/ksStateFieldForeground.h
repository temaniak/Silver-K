#pragma once
#include "ksStateFieldService.h"
#include "ksStateScreenForeground.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StateFieldForeground final {
public:
 using CallerAbi=StateScreenForeground::CallerAbi;
 bool step(ControllerCpu&);
 unsigned active()const{return job_?1:0;}const StateFieldService*service()const{return job_?&job_->service:nullptr;}
 bool at(StateFieldService::Kind k)const{return service()&&service()->kind()==k;}bool at(StateFieldService::Component c)const{return service()&&service()->component()==c;}
 template<class T>const T*task()const{return service()?service()->task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 static void projectUi(CallerAbi&,const StateFieldService&);
 static void projectApply(CallerAbi&,const StateApplyService&,uint32_t recipe=0);
 static void projectScene(CallerAbi&,const StateFieldService&);
 static void projectSceneEdit(CallerAbi&,const StateFieldService&);
 static void project(CallerAbi&,const StateFieldService&,StateFieldService::Input,uint32_t handler=0,uint32_t recipe=0);
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,25>kinds{};
private:
 struct Job{Job(StateFieldService::Kind k,StateFieldService::Input i,uint32_t c,int d,uint8_t p):service(k,i),input(i),caller(c),depth(d),priority(p){}StateFieldService service;StateFieldService::Input input;uint32_t caller=0,handler=0,recipe=0;int depth=0;uint8_t priority=0;};
 std::optional<Job>job_;
};}
