#pragma once
#include "ksSerialForegroundService.h"
#include <array>
#include <cstdint>
#include <optional>
namespace ks {
class ControllerCpu;
// Narrow return edge to callers still using the migration ABI. The owned
// graph itself never reads or writes execution registers or a caller stack.
class SerialForeground final {
public:
 bool step(ControllerCpu&);
 bool active()const{return job_.has_value();}
 bool at(SerialForegroundService::Kind kind,bool requireCritical=false)const{return job_&&job_->service.kind()==kind&&(!requireCritical||job_->service.critical());}
 bool atQueue(SerialQueueTask::Stage stage)const{return job_&&job_->service.atQueue(stage);}
 bool atReceive(SerialReceiveTask::Stage stage)const{return job_&&job_->service.atReceive(stage);}
 bool atTransmit(SerialTransmitTask::Stage stage)const{return job_&&job_->service.atTransmit(stage);}
 void resetMetrics(){started=completed=phases=0;initial=active();kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;std::array<uint64_t,5>kinds{};
private:
 struct Job{
  SerialForegroundService service;uint32_t sp;uint8_t priority;int depth;
  Job(SerialForegroundService::Kind kind,uint8_t input,uint32_t stack,uint8_t exr,int level):service(kind,input),sp(stack),priority(exr),depth(level){}
 };
 std::optional<Job>job_;
};
}
