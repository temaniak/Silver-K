#pragma once
#include "kstationControllerInterruptScheduler.h"
#include <array>
#include <optional>
namespace kstation {
// Semantic IRQ ownership. The board scheduler must settle admission and its
// live SYSCR/vector/context timing before advancing a service. Any pending
// IRQ requires a fresh SYSCR observation at each phase; an unmasked request
// must be admitted before resuming. The board must drive
// the explicitly requested Timer1/RX/TX/ADC children. No CPU frame is held.
class ControllerInterruptOwner final {
 using Scheduler=ControllerInterruptScheduler;
public:
 using Kind=Scheduler::Kind;using Mask=Scheduler::Mask;using Admission=Scheduler::Admission;
 struct Advance{unsigned clocks=0;bool returned=false;Kind kind=Kind::Unsupported;};
 bool bindVerifiedVectors(const uint8_t*image,size_t size){return scheduler_.bindVerifiedVectors(image,size);}bool bound()const{return scheduler_.bound();}Scheduler::Selection select(uint64_t p)const{return scheduler_.select(p);}void reset(){scheduler_.reset();for(auto&j:jobs_)j.reset();}bool active()const{return scheduler_.active();}unsigned depth()const{return scheduler_.depth();}Mask mask()const{return scheduler_.mask();}void setMask(Mask m){scheduler_.setMask(m);}Kind current()const{return scheduler_.current();}
 Admission admit(uint64_t&pending,bool syscrObserved,bool mode2=false,uint64_t high=0){auto a=scheduler_.admit(pending,syscrObserved,mode2,high);if(a.result!=Scheduler::Admit::Entered)return a;auto&j=jobs_[depth()-1];j.emplace();j->kind=a.kind;if(a.kind==Kind::Timer3)j->task.emplace(ControllerInterruptTask::Kind::Timer3);else if(a.kind==Kind::SerialError)j->task.emplace(ControllerInterruptTask::Kind::SerialError);else if(a.kind==Kind::Unused)j->task.emplace(ControllerInterruptTask::Kind::Unused);return a;}
 bool awaitingAdmission()const{return active()&&!jobs_[depth()-1]->admissionSettled;}bool settleAdmission(uint32_t liveTarget){if(!awaitingAdmission()||!Scheduler::liveVectorMatches(current(),liveTarget))return false;jobs_[depth()-1]->admissionSettled=true;return true;}
 const ControllerInterruptTask* currentTask()const{if(!active())return nullptr;const auto&j=*jobs_[depth()-1];return j.task?&*j.task:nullptr;}
 bool awaitingExternal()const{return active()&&!awaitingAdmission()&&!jobs_[depth()-1]->task.has_value();}
 bool completeExternal(){if(!awaitingExternal())return false;pop();return true;}
 unsigned nextClocks(const ControllerInterruptTask::Credits&credits)const{if(!active()||awaitingAdmission())return 0;const auto&j=*jobs_[depth()-1];return j.task?j.task->nextClocks(credits):0;}
 template<class Reader,class Writer>Advance advanceBefore(unsigned available,const ControllerInterruptTask::Credits&credits,Reader&&read,Writer&&write,uint64_t pending=0,bool syscrObserved=false,bool mode2=false,uint64_t high=0){if(mode2||high||(pending&&(!syscrObserved||mask()==Mask::Enabled))||!active()||awaitingAdmission())return {};auto&j=*jobs_[depth()-1];if(!j.task)return {};auto cost=j.task->nextClocks(credits);if(!cost||cost>available)return {};auto kind=j.kind;auto used=j.task->advanceBefore(cost,credits,read,write);bool done=j.task->complete();if(done)pop();return {used,done,kind};}
private:
 struct Job{Kind kind=Kind::Unsupported;std::optional<ControllerInterruptTask>task{};bool admissionSettled=false;};
 void pop(){jobs_[depth()-1].reset();scheduler_.complete();}
 Scheduler scheduler_{};std::array<std::optional<Job>,Scheduler::Capacity>jobs_{};
};
}
