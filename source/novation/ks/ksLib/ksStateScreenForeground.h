#pragma once
#include "ksStateScreenService.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StateScreenForeground final {
public:
 const StateScreenService*service()const{return job_?&job_->service:nullptr;}
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}
 bool at(StateScreenService::Kind k)const{return job_&&job_->service.kind()==k;}bool at(StateScreenService::Component c)const{return job_&&job_->service.component()==c;}
 template<class T>const T*task()const{return job_?job_->service.template task<T>():nullptr;}
 bool at(StateScreenTask::Stage s)const{const auto*t=task<StateScreenTask>();return t&&t->stage()==s;}
 bool at(StateScreenEditTask::Stage s)const{const auto*t=task<StateScreenEditTask>();return t&&t->stage()==s;}
 void resetMetrics(){initial=initialActive=active();started=completed=phases=0;components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0,initialActive=0;uint32_t components=0;std::array<uint64_t,10>kinds{};
 // This representation exists only at the final external ABI boundary. Native
 // tasks neither construct it nor update it while a transaction is running.
 struct CallerAbi {std::array<uint32_t,7>values{};uint8_t flags=0;};
 static void projectValue(CallerAbi&,const StateScreenService::ValueResult&);
 static void projectRender(CallerAbi&,const StateScreenService::Summary&);
private:
 struct Job {Job(StateScreenService::Kind k,StateScreenService::Input i,uint32_t c,int d,uint8_t p):service(k,i),caller(c),depth(d),priority(p){}StateScreenService service;uint32_t caller;int depth;uint8_t priority;};
 std::optional<Job>job_;
};}
