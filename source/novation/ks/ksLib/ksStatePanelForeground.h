#pragma once
#include "ksStatePanelService.h"
#include <array>
namespace ks {
class ControllerCpu;
class StatePanelForeground final {
public:
 bool step(ControllerCpu&);bool active()const{return job_.has_value();}
 bool at(StatePanelService::Kind k)const{return job_&&job_->service.kind()==k;}bool at(StatePanelService::Component c)const{return job_&&job_->service.component()==c;}
 bool atKeyboard(KeyboardEventsTask::Stage s)const{return job_&&job_->service.atKeyboard(s);}
 bool atRoute(MidiOutputTask::Stage s)const{return job_&&job_->service.atRoute(s);}
 bool atPacket(MidiOutputTask::Stage s)const{return job_&&job_->service.atPacket(s);}
 bool atLocal(MidiOutputTask::Stage s)const{return job_&&job_->service.atLocal(s);}
 bool atQueue(InternalMidiQueueTask::Stage s)const{return job_&&job_->service.atQueue(s);}
 bool atSerial(SerialTransmitTask::Stage s)const{return job_&&job_->service.atSerial(s);}
 void resetMetrics(){started=completed=phases=0;initial=active();components=0;kinds.fill(0);}
 uint64_t started=0,completed=0,phases=0,initial=0;uint32_t components=0;std::array<uint64_t,1>kinds{};
private:
 struct Job{StatePanelService service;uint32_t caller;int depth;uint8_t priority;Job(uint32_t sp,int level,uint8_t exr):caller(sp),depth(level),priority(exr){}};
 std::optional<Job>job_;
};
}
