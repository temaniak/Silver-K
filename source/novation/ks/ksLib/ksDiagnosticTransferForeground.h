#pragma once
#include "ksDiagnosticTransferService.h"
#include <array>
namespace ks {class ControllerCpu;
class DiagnosticTransferForeground final {public:
 bool step(ControllerCpu&);bool active()const{return job_.has_value();}const DiagnosticTransferService*service()const{return job_?&job_->service:nullptr;}
 void resetMetrics(){initial=active();started=completed=phases=0;}uint64_t initial=0,started=0,completed=0,phases=0;
private:struct Job{DiagnosticTransferService service;uint32_t caller;int depth;Job(DiagnosticTransferService::Kind k,DiagnosticTransferService::Input i,uint32_t c,int d):service(k,i),caller(c),depth(d){}};std::optional<Job>job_;
};}
