#pragma once
#include "ksVoiceFeedbackService.h"
#include <array>
namespace ks {class ControllerCpu;
class VoiceFeedbackForeground final {
public:
 struct CallerAbi{std::array<uint32_t,7>values{};uint8_t flags=0;};
 bool step(ControllerCpu&);bool active()const{return bool(job_);}
 const VoiceFeedbackService*service()const{return job_?&job_->service:nullptr;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 static void project(CallerAbi&,const VoiceFeedbackService&);
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);}
 uint64_t initial=0,started=0,completed=0,phases=0;uint32_t components=0;std::array<uint64_t,2>kinds{};
private:
 struct Job{VoiceFeedbackService service;uint32_t caller;int depth;uint8_t priority;Job(VoiceFeedbackService::Kind k,uint32_t c,int d,uint8_t p):service(k),caller(c),depth(d),priority(p){}};
 std::optional<Job>job_;
};}
