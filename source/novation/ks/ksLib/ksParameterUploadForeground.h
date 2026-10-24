#pragma once
#include "ksParameterUploadService.h"
#include <array>
#include <optional>
namespace ks {class ControllerCpu;
class ParameterUploadForeground final {
public:
 bool step(ControllerCpu&);unsigned active()const{return job_?1u:0u;}const ParameterUploadService*service()const{return job_?&job_->service:nullptr;}
 template<class T>const T*task()const{return job_?job_->service.template task<T>():nullptr;}
 bool at(ParameterUploadService::Component c)const{return job_&&job_->service.component()==c;}bool at(ParameterUploadService::Step s)const{return job_&&job_->service.step()==s;}
 void resetMetrics(){initial=initialActive=active();started=completed=phases=0;components=groups=0;}
 uint64_t started=0,completed=0,phases=0,initial=0,initialActive=0;uint32_t components=0,groups=0;
 // Constructed once at the final external return. No native task owns or
 // updates this view while parameter transactions are in progress.
 struct CallerAbi{std::array<uint32_t,7>values{};uint8_t flags=0;};
 static void project(CallerAbi&,const ParameterUploadService::Summary&);
private:
 struct Job{Job(ParameterUploadService::Input in,uint32_t sp,int d,uint8_t p):service(in),caller(sp),depth(d),priority(p){}ParameterUploadService service;uint32_t caller;int depth;uint8_t priority;};
 std::optional<Job>job_;
};
}
