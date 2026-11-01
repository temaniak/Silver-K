#pragma once
#include "ksStateMenuService.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class StateMenuForeground final {
public:
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}
 bool at(StateMenuService::Kind kind)const{return job_&&job_->service.kind()==kind;}
 bool at(StateMenuService::Component component)const{return job_&&job_->service.component()==component;}
 void resetMetrics(){initial=initialActive=active();started=completed=phases=0;components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0,initialActive=0;uint32_t components=0;std::array<uint64_t,14>kinds{};
private:
 struct Job {StateMenuService service;uint32_t caller;int depth;Job(StateMenuService::Kind kind,StateMenuService::Input input,uint32_t stack,int level):service(kind,input),caller(stack),depth(level){}};
 std::optional<Job>job_;
};
}
