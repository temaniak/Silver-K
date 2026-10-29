#pragma once
#include "kstationMidiEventTask.h"
#include <optional>
namespace kstation {
class ControllerVoiceLongTiming final {
 using T=MidiVoiceTimingTask;using S=T::Stage;using F=MidiFloatConvertTask;using FS=F::Stage;
public:
 explicit ControllerVoiceLongTiming(uint32_t patch):patch_(patch){}bool complete()const{return task_.awaitingChild()&&task_.child()==T::Child::ContinueLong;}uint32_t result()const{return result_;}bool stackWrites()const{return !floating_&&task_.stage()==S::Convert;}
 unsigned nextClocks()const{if(floating_){switch(floating_->stage()){case FS::Complete:return 0;case FS::CompareMinimum:return 6;case FS::Return:return 7;default:return 3;}}switch(task_.stage()){case S::AwaitChild:case S::Complete:return 0;case S::ReadInput:return 12+bus(patch_+3,1);case S::ReadPatchType:return 12+bus(patch_+0x80,1);case S::ReadMode:return 12;case S::ReadOffset:return 12;case S::Multiply:return 15;case S::Convert:return 10;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;if(floating_){if(!floating_->advance())return 0;if(floating_->complete()){result_=floating_->result();floating_.reset();task_.resume(T::Child::Convert);}return cost;}bool used=task_.advance([&](T::Read f){uint32_t a=f==T::Read::Input?patch_+3:f==T::Read::PatchType?patch_+0x80:f==T::Read::Mode?0xd02c:0xbfa6;return read(a&0xffffff,f==T::Read::Offset?2:1,cost);});if(!used)return 0;if(task_.awaitingChild()&&task_.child()==T::Child::Convert)floating_.emplace(task_.input());return cost;}
private:static unsigned bus(uint32_t a,unsigned n){int p=-1;unsigned c=0;for(unsigned i=0;i<n;++i){auto x=(a+i)&0xffffff;if(x<0x400000||(x>=0xfffd10&&x<0xffffa0)){if((p&1)||x!=uint32_t(p|1))c+=x<0x400000?3:1;p=int(x);}else{c+=x>=0xffffa0?1:3;p=-1;}}return c;}uint32_t patch_,result_=0;T task_{};std::optional<F>floating_{};
};
}
