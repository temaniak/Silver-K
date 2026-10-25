#pragma once
#include "ksNativeVoiceReleaseService.h"
#include <array>
namespace ks {class ControllerCpu;
class NativeVoiceReleaseForeground final {
public:
 bool step(ControllerCpu&);bool active()const{return bool(job_);}
 const NativeVoiceReleaseService*service()const{return job_?&job_->service:nullptr;}
 template<class T>const T*task()const{
  if(!service())return nullptr;
  if constexpr(std::is_same_v<T,VoiceReleaseTask>)return service()->component()==NativeVoiceReleaseService::Component::Voices?service()->voiceTask():nullptr;
  else if constexpr(std::is_same_v<T,PartLookupTask>)return service()->lookupTask();
  else if constexpr(std::is_same_v<T,DspRoutingTask>)return service()->routingTask();
  else if constexpr(std::is_same_v<T,DspAddressTask>)return service()->addressTask();
  else if constexpr(std::is_same_v<T,DspWordTask>)return service()->wordTask();
  else if constexpr(std::is_same_v<T,DspFaultTask>)return service()->faultTask();
  else if constexpr(std::is_same_v<T,KeyResetTask>)return service()->keyTask();
  else return nullptr;
 }
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;std::array<uint64_t,5>kinds{};
private:
 struct Job{NativeVoiceReleaseService service;uint32_t caller;int depth;uint8_t priority;Job(VoiceReleaseTask::Mode m,NativeVoiceReleaseService::Input in,uint32_t c,int d,uint8_t p):service(m,in),caller(c),depth(d),priority(p){}};std::optional<Job>job_;
};}
