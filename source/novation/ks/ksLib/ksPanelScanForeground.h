#pragma once
#include "ksPanelScanService.h"
#include <array>
#include <optional>
namespace ks {
class ControllerCpu;
class PanelScanForeground final {
public:
 bool step(ControllerCpu&);
 bool active()const{return job_.has_value();}
 bool at(PanelScanService::Component component)const{return job_&&job_->service.component()==component;}
 bool atMatrix(PanelMatrixTask::Stage stage)const{return job_&&job_->service.atMatrix(stage);}
 bool atPush(PanelInputTask::Stage stage)const{return job_&&job_->service.atPush(stage);}
 bool atReset(PanelInputTask::Stage stage)const{return job_&&job_->service.atReset(stage);}
 void resetMetrics(){started=completed=phases=0;initial=active();components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,3>kinds{};
private:
 struct Job{PanelScanService service;uint32_t caller;int depth;
  Job(PanelScanService::Kind kind,uint16_t event,uint32_t stack,int level):service(kind,event),caller(stack),depth(level){}
 };
 std::optional<Job>job_;
};
}
