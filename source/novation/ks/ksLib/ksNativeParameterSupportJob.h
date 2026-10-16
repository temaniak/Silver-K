#pragma once
#include "ksNativeParameterJob.h"
#include "ksNativeDspRoutingJob.h"
#include "ksParameterSupportTask.h"
namespace ks {
// Native rate, clock, sync and final routing jobs. Inline blocks finish at the
// last child completion; only Exit includes its own callable return interval.
class NativeParameterSupportJob final {
 using T=ParameterSupportTask;using S=T::Stage;
public:
 NativeParameterSupportJob(T::Kind kind,uint16_t rateField,NativeDspContext&context):task_(kind),field_(rateField),context_(context){}
 bool complete()const{return task_.complete();}
 unsigned nextClocks()const{if(complete())return 0;if(fault_)return fault_->nextClocks();if(word_)return word_->nextClocks();if(coefficient_)return coefficient_->nextClocks();if(select_)return select_->stage()==DspSelectTask::Stage::Complete?17:select_->nextClocks();return task_.nextClocks();}
 template<class IO>NativeDspJob::Progress advanceBefore(unsigned available,IO&io){
  const auto cost=nextClocks();if(!cost||cost>available)return {};NativeDspJob::Progress out{cost};
  if(fault_){out=fault_->advanceBefore(cost,[&](DspFaultTask::Read r,unsigned at){return io.readFault(r,at);},[&](DspFaultTask::Write w,uint8_t v,unsigned at){io.writeFault(w,v,at);});if(fault_->complete()){fault_.reset();word_->resumeFault();}out.completed=false;return out;}
  if(word_){out=word_->advanceBefore(cost,[&](NativeDspJob::Port p,unsigned off,unsigned at){return io.readDsp(p,off,at);},[&](NativeDspJob::Port p,unsigned off,uint8_t v,unsigned at){io.writeDsp(p,off,v,at);},[](uint16_t,unsigned){});if(out.faultRequested){fault_.emplace();out.faultRequested=false;}else if(word_->complete()){word_.reset();task_.resumeChild();}out.completed=complete();return out;}
  if(coefficient_){out=coefficient_->advanceBefore(cost);if(coefficient_->complete()){auto value=uint32_t(coefficient_->value());coefficient_.reset();task_.resumeChild(value);}out.completed=false;return out;}
  if(select_){if(select_->stage()==DspSelectTask::Stage::Complete){select_.reset();task_.resumeChild();}else{if(select_->stage()==DspSelectTask::Stage::Select)context_.port=select_->port()==DspPort::A?NativeDspJob::Port::A:NativeDspJob::Port::B;select_->advanceBefore(cost,[&](DspPort,uint16_t value){context_.cached=value;io.writeRoute(DspRoutingTask::Write::CachedAddress,value,cost);});}return out;}
  task_.advanceBefore(cost,[&](T::Read what){return io.readSupport(what,field_,cost);});
  switch(task_.stage()){
   case S::AwaitCoefficient:coefficient_.emplace(int16_t(task_.scaleInput()));break;
   case S::AwaitExitFirst:select_.emplace(DspPort::A);break;case S::AwaitExitSecond:select_.emplace(DspPort::B);break;
   case S::AwaitRate:case S::AwaitClock:case S::AwaitSyncFirst:case S::AwaitSyncLast:word_.emplace(NativeDspJob::Word{task_.dspWord()},context_.port);break;default:break;
  }out.completed=complete();return out;
 }
private:T task_;uint16_t field_;NativeDspContext&context_;std::optional<NativeDspJob>word_;std::optional<NativeDspFaultJob>fault_;std::optional<NativeCoefficientJob>coefficient_;std::optional<DspSelectTask>select_;
};}
