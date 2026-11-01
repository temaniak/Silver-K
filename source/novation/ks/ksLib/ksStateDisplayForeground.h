#pragma once
#include "ksStateDisplayService.h"
#include <array>
namespace ks {class ControllerCpu;
class StateDisplayForeground final {
public:
 bool step(ControllerCpu&);bool active()const{return job_.has_value();}
 bool at(StateDisplayService::Kind k)const{return job_&&job_->service.kind()==k;}bool at(StateDisplayService::Component c)const{return job_&&job_->service.component()==c;}
 bool atDecimal(PanelDecimalTask::Stage s)const{return job_&&job_->service.atDecimal(s);}
 bool atAction(PanelActionTask::Kind k,PanelActionTask::Stage s)const{return job_&&job_->service.atAction(k,s);}
 bool atByte(PanelByteTask::Kind k,PanelByteTask::Stage s)const{return job_&&job_->service.atByte(k,s);}
 bool atFormat(StateMenuFormatTask::Stage s)const{return job_&&job_->service.atFormat(s);}bool atNumber(PanelFormatTask::Stage s)const{return job_&&job_->service.atNumber(s);}bool atText(PanelTextTask::Stage s)const{return job_&&job_->service.atText(s);}bool atParse(PanelTextTask::Stage s)const{return job_&&job_->service.atParse(s);}bool atAction(PanelActionTask::Stage s)const{return job_&&job_->service.atAction(s);}bool atPoll(PanelPollTask::Stage s)const{return job_&&job_->service.atPoll(s);}bool atByte(PanelByteTask::Stage s)const{return job_&&job_->service.atByte(s);}
 void resetMetrics(){started=completed=phases=0;initial=active();components=0;kinds.fill(0);}uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,5>kinds{};
private:
 struct Job{StateDisplayService service;uint32_t caller;int depth;Job(StateDisplayService::Kind k,StateDisplayService::Input in,uint32_t sp,int level):service(k,in),caller(sp),depth(level){}};std::optional<Job>job_;
};}
