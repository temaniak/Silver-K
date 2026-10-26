#pragma once
#include "ksNativeArpOwnedService.h"
#include "ksNativeArpOwnedCompatibility.h"
#include <array>
namespace ks {class ControllerCpu;
class NativeArpOwnedForeground final {
public:
 bool step(ControllerCpu&);bool active()const{return bool(job_);}
 const NativeArpOwnedService*owned()const{return job_?&job_->service:nullptr;}
 const NativeArpMainService*service()const{return job_?job_->service.arp().main():nullptr;}
 template<class T>const T*task()const{return owned()?owned()->template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;std::array<uint64_t,3>kinds{};
private:
 struct Job{NativeArpOwnedService service;NativeArpOwnedCompatibility compatibility;uint32_t caller;int depth;uint8_t priority;template<class A>Job(NativeArpOwnedService::Kind kind,NativeArpOwnedService::Input in,const A&a,uint32_t c,int d,uint8_t p):service(kind,in),compatibility(kind,a),caller(c),depth(d),priority(p){}};std::optional<Job>job_;
};}
