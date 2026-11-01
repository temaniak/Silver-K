#pragma once
#include "ksStateEncoderService.h"
#include "ksStateEncoderAbi.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StateEncoderForeground final {
public:
 using CallerAbi=StateFieldForeground::CallerAbi;
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}
 const StateEncoderService*service()const{return job_?&job_->service:nullptr;}
 bool at(StateEncoderService::Component c)const{return service()&&service()->component()==c;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=0;components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,1>kinds{};
 static void project(CallerAbi&a,const StateEncoderService&s){projectStateEncoder(a,s);}
private:
 struct Job{Job(StateEncoderService::Input in,uint32_t s,int d,uint8_t p):service(StateEncoderService::Kind::Encoder,in),caller(s),depth(d),priority(p){}StateEncoderService service;uint32_t caller;int depth;uint8_t priority;};std::optional<Job>job_;
};}
