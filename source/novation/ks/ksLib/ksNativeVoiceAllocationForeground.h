#pragma once
#include "ksNativeVoiceAllocationService.h"
#include <array>
namespace ks {class ControllerCpu;
class NativeVoiceAllocationForeground final {
public:
 bool step(ControllerCpu&);bool active()const{return bool(job_);}
 const NativeVoiceAllocationService*service()const{return job_?&job_->service:nullptr;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;std::array<uint64_t,2>kinds{};
private:
 struct Job{NativeVoiceAllocationService service;uint32_t caller;int depth;uint8_t priority;Job(NativeVoiceAllocationService::Kind k,NativeVoiceAllocationService::Input in,uint32_t c,int d,uint8_t p):service(k,in),caller(c),depth(d),priority(p){}};std::optional<Job>job_;
};}
