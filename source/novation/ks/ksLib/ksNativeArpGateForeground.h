#pragma once
#include "ksNativeArpGateService.h"
#include <array>
namespace ks {class ControllerCpu;
class NativeArpGateForeground final {
public:
 bool step(ControllerCpu&);bool active()const{return bool(job_);}
 const NativeArpGateService*service()const{return job_?&job_->service:nullptr;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;std::array<uint64_t,3>kinds{};
private:
 struct Job{NativeArpGateService service;uint32_t caller;int depth;uint8_t priority;Job(NativeArpGateService::Kind k,NativeArpGateService::Input in,uint32_t c,int d,uint8_t p):service(k,in),caller(c),depth(d),priority(p){}};std::optional<Job>job_;
};}
