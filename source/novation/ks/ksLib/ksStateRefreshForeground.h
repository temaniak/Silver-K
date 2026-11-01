#pragma once
#include "ksStateRefreshService.h"
#include "ksStateSaveAbi.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class StateRefreshForeground final {
public:
 using CallerAbi=StateSaveCallerAbi;
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}const StateRefreshService*service()const{return job_?&job_->service:nullptr;}
 bool at(StateRefreshService::Kind k)const{return service()&&service()->kind()==k;}bool at(StateRefreshService::Component c)const{return service()&&service()->component()==c;}
 template<class T>const T*task()const{return service()?service()->template task<T>():nullptr;}
 static void project(CallerAbi&,const StateRefreshService&);
 void resetMetrics(){initial=active();started=completed=phases=components=0;kinds.fill(0);returnedPacket=returnedPanel=returnedDeferred=returnedClock=processedParts=returnedCoefficient=returnedPeriodAddress=returnedPeriodValue=0;}uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,1>kinds{};
 // Completed-child observations attached to real outer returns, never admissions.
 uint64_t returnedPacket=0,returnedPanel=0,returnedDeferred=0,returnedClock=0,processedParts=0,returnedCoefficient=0,returnedPeriodAddress=0,returnedPeriodValue=0;
private:struct Job{Job(uint32_t c,int d):caller(c),depth(d){}StateRefreshService service;uint32_t caller;int depth;};std::optional<Job>job_;
};}
