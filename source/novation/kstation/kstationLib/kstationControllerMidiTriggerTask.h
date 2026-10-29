#pragma once
#include "kstationMidiEventTask.h"
#include "kstationControllerMidiDspTriggerTask.h"
#include <optional>
namespace kstation {
class ControllerMidiTriggerTask final {
 using T=MidiTriggerTask;using S=T::Stage;
public:
 explicit ControllerMidiTriggerTask(uint8_t note):task_(note){}bool complete()const{return task_.complete();}bool awaitingVoice()const{return task_.awaitingChild()&&task_.child()==T::Child::StartVoice;}bool resumeVoice(){return awaitingVoice()&&task_.resume(T::Child::StartVoice);}bool awaitingDspTail()const{return dsp_&&dsp_->awaitingTail();}bool resumeDspTail(){if(!awaitingDspTail()||!dsp_->resumeTail())return false;dsp_.reset();return task_.resume(T::Child::TailPublish);}uint8_t triggerValue()const{return dsp_?dsp_->triggerValue():0;}bool stackWrites()const{return dsp_?dsp_->stackWrites():task_.stage()==S::StartVoice;}
 unsigned nextClocks()const{if(dsp_)return dsp_->nextClocks();switch(task_.stage()){case S::WriteLast:return 12;case S::StartVoice:return 10;case S::TailPublish:return 8;default:return 0;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write){auto cost=nextClocks();if(!cost||cost>budget)return 0;if(dsp_){auto used=dsp_->advanceBefore(cost,read,write);if(dsp_->complete()){dsp_.reset();task_.resume(T::Child::TailPublish);}return used;}bool ok=task_.advance([&](uint8_t v){write(0xcbfe,v,1,cost);});if(!ok)return 0;if(task_.awaitingChild()&&task_.child()==T::Child::TailPublish)dsp_.emplace(MidiDspTriggerTask::Kind::Note);return cost;}
private:T task_;std::optional<ControllerMidiDspTriggerTask>dsp_{};
};
}
