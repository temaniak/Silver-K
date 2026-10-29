#pragma once
#include "kstationMidiEventTask.h"
#include "kstationControllerVoiceDspWordTask.h"
#include "kstationControllerVoiceBuildTask.h"
#include <optional>
namespace kstation {
class ControllerVoiceLongFinal final {
 using T=MidiVoiceFinalTask;using S=T::Stage;using B=MidiVoiceBuildTask;
public:
 explicit ControllerVoiceLongFinal(uint32_t patch):patch_(patch){}bool complete()const{return task_.complete();}bool awaitingDsp()const{return dsp_?dsp_->awaitingDsp():shared_&&shared_->awaitingDsp();}uint16_t address()const{return dsp_?dsp_->address():shared_?shared_->dspAddress():0;}uint32_t word()const{return dsp_?dsp_->word():shared_?shared_->word():0;}bool resumeDsp(){if(!awaitingDsp())return false;return dsp_?dsp_->resumeDsp():shared_->resume(B::Child::SendCommand);}
 bool stackWrites()const{if(dsp_)return dsp_->stackWrites();if(shared_)return shared_->stackWrites();auto s=task_.stage();return s==S::SendFirst||s==S::SendFinal;}
 unsigned nextClocks()const{if(dsp_)return dsp_->nextClocks();if(shared_)return shared_->nextClocks();auto s=task_.stage();switch(s){case S::AwaitChild:case S::Complete:return 0;case S::ReadNote:case S::ReadMode:return 12;case S::ReadPatchHigh:return 12+bus(patch_+0x4f);case S::ReadPatchLow:return 12+bus(patch_+0x4e);case S::ReadPatchFlags:return 12+bus(patch_+0x50);case S::ReadTranspose:return 12+bus(patch_+4);case S::BindFirst:case S::BindFinal:return 6;case S::SendFirst:case S::SendFinal:return 10;case S::SelectMode:return 8;case S::Return:return 7;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;if(dsp_){auto used=dsp_->advanceBefore(cost,read);if(dsp_->complete()){auto child=task_.child();dsp_.reset();task_.resume(child);}return used;}if(shared_){auto used=shared_->advanceBefore(cost,read);if(shared_->complete()){shared_.reset();task_.resume(T::Child::SharedFinal);}return used;}bool used=task_.advance([&](T::Read f){uint32_t a=f==T::Read::Note?0xd02f:f==T::Read::Mode?0xd02c:patch_+(f==T::Read::PatchHigh?0x4f:f==T::Read::PatchLow?0x4e:f==T::Read::PatchFlags?0x50:4);auto v=uint8_t(read(a&0xffffff,1,cost));if(f==T::Read::Note)note_=v;else if(f==T::Read::PatchHigh)high_=v;else if(f==T::Read::PatchLow)low_=v;else if(f==T::Read::PatchFlags)flags_=v;else if(f==T::Read::Transpose)transpose_=v;return v;});if(!used)return 0;if(task_.awaitingChild()){if(task_.child()==T::Child::SendFirst)dsp_.emplace(0x6a,uint32_t(uint8_t(note_+0xc4))<<16);else if(task_.child()==T::Child::SendFinal){auto flags=uint8_t((flags_&7)|0xb8|(uint8_t(transpose_-64)?0x40:0));auto word=(uint32_t(high_)<<16)|(uint32_t(low_&3)<<8)|flags;dsp_.emplace(0x1a,word);}else shared_.emplace(patch_,true);}return cost;}
private:static unsigned bus(uint32_t a){return (a&0xffffff)<0xfffd10?3:1;}uint32_t patch_;uint8_t note_=0,high_=0,low_=0,flags_=0,transpose_=0;T task_{};std::optional<ControllerVoiceDspWordTask>dsp_{};std::optional<ControllerVoiceBuildTask>shared_{};
};
}
