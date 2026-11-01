#pragma once
#include "ksStateStoreService.h"
#include <array>
namespace ks {
class ControllerCpu;
class StateStoreForeground final {
public:
 bool step(ControllerCpu&);
 bool active()const{return job_.has_value();}
 bool at(StateStoreService::Kind kind)const{return job_&&job_->service.kind()==kind;}
 bool at(StateStoreService::Component component)const{return job_&&job_->service.component()==component;}
 void resetMetrics(){started=completed=phases=0;initial=active();components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,17>kinds{};
private:
 struct Job{StateStoreService service;uint32_t caller,descriptor=0;int depth;Job(StateStoreService::Kind k,StateStoreService::Input input,uint32_t stack,int level):service(k,input),caller(stack),depth(level){}};
 std::optional<Job>job_;
};
}
