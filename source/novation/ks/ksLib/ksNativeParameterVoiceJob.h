#pragma once
#include "ksNativeDspJob.h"
#include "ksPartControlTasks.h"
#include "ksParameterPitchTask.h"
#include "ksParameterVoiceScaleTask.h"
#include <optional>
namespace ks {
// Per-voice pitch and amplitude calculations, including the late voice lookup
// and selected patch resource. Handles are musical data, never caller state.
class NativeParameterVoiceJob final {
public:
 struct Pitch {unsigned output;};struct Amplitude {uint16_t field;};
 NativeParameterVoiceJob(Pitch request,NativeDspJob::Port port):task_(std::in_place_type<ParameterPitchTask>,request.output),field_(0),port_(port){}
 NativeParameterVoiceJob(Amplitude request,NativeDspJob::Port port):task_(std::in_place_type<ParameterVoiceScaleTask>),field_(request.field),port_(port){}
 bool complete()const{return std::visit([](const auto&t){return t.complete();},task_);}
 unsigned nextClocks()const{if(complete())return 0;if(fault_)return fault_->nextClocks();if(word_)return word_->nextClocks();if(lookup_)return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();return std::visit([](const auto&t){return t.nextClocks();},task_);}
 template<class IO>NativeDspJob::Progress advanceBefore(unsigned available,IO&io){
  const auto cost=nextClocks();if(!cost||cost>available)return {};NativeDspJob::Progress out{cost};
  if(fault_){out=fault_->advanceBefore(cost,[&](DspFaultTask::Read r,unsigned at){return io.readFault(r,at);},[&](DspFaultTask::Write w,uint8_t v,unsigned at){io.writeFault(w,v,at);});if(fault_->complete()){fault_.reset();word_->resumeFault();}out.completed=false;return out;}
  if(word_){out=word_->advanceBefore(cost,[&](NativeDspJob::Port p,unsigned off,unsigned at){return io.readDsp(p,off,at);},[&](NativeDspJob::Port p,unsigned off,uint8_t v,unsigned at){io.writeDsp(p,off,v,at);},[](uint16_t,unsigned){});if(out.faultRequested){fault_.emplace();out.faultRequested=false;}else if(word_->complete()){word_.reset();std::visit([](auto&t){t.resumeWord();},task_);}out.completed=complete();return out;}
  if(lookup_){if(lookup_->stage()==PartLookupTask::Stage::Complete){auto slot=lookup_->index();lookup_.reset();std::visit([&](auto&t){t.resumeVoice(slot);},task_);}else lookup_->advanceBefore(cost,[&](PartLookupTask::Layout){return io.readVoiceIndex(cost);});return out;}
  std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;
   if constexpr(std::is_same_v<T,ParameterPitchTask>)t.advanceBefore(cost,[&](typename T::Read what){auto value=io.readPitch(what,t.output(),t.slot(),selectedPatch_,cost);if(what==T::Read::SelectedPatch)selectedPatch_=value;return value;});
   else t.advanceBefore(cost,[&](typename T::Read what){auto value=io.readAmplitude(what,field_,t.slot(),selectedPatch_,cost);if(what==T::Read::Patch)selectedPatch_=value;return value;});
   if(t.awaitingVoice())lookup_.emplace(PartLookupTask::Layout::Voice);
   if(t.awaitingWord())word_.emplace(NativeDspJob::Word{t.dspWord()},port_);
  },task_);out.completed=complete();return out;
 }
private:std::variant<ParameterPitchTask,ParameterVoiceScaleTask>task_;uint16_t field_;NativeDspJob::Port port_;uint32_t selectedPatch_=0;std::optional<PartLookupTask>lookup_;std::optional<NativeDspJob>word_;std::optional<NativeDspFaultJob>fault_;
};}
