#pragma once
#include "ksStateTickService.h"
#include "ksStateSaveAbi.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StateTickForeground final {
public:
 using CallerAbi=StateSaveCallerAbi;
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}
 const StateTickService*service()const{return job_?&job_->service:nullptr;}
 bool at(StateTickService::Kind k)const{return service()&&service()->kind()==k;}
 bool at(StateTickService::Component c)const{return service()&&service()->component()==c;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 static void project(CallerAbi&,const StateTickService&);
 void resetMetrics(){initial=active();started=completed=phases=components=returnedClock=returnedButtons=returnedAdc=returnedEncoder=returnedPedal=returnedOutput=returnedRefresh=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0,returnedClock=0,returnedButtons=0,returnedAdc=0,returnedEncoder=0,returnedPedal=0,returnedOutput=0,returnedRefresh=0;
 uint32_t components=0;std::array<uint64_t,1>kinds{};
private:
 struct Job{Job(uint32_t c,int d,uint8_t p):caller(c),depth(d),priority(p){}StateTickService service;uint32_t caller;int depth;uint8_t priority;};
 std::optional<Job>job_;
};}
