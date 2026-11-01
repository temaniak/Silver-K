#pragma once
#include "kstationMidiEventTask.h"
#include "kstationVoiceAllocationTask.h"
#include "kstationControllerVoiceDspWordTask.h"
#include <optional>
namespace kstation {
class ControllerVoiceLongGlide final {
 using T=MidiVoiceGlideTask;using S=T::Stage;using V=VoiceAddressTask;
public:
 explicit ControllerVoiceLongGlide(uint32_t patch):patch_(patch){}bool complete()const{return task_.awaitingChild()&&task_.child()==T::Child::ContinueLong;}bool awaitingDsp()const{return dsp_&&dsp_->awaitingDsp();}uint16_t address()const{return dsp_?dsp_->address():0;}uint32_t word()const{return dsp_?dsp_->word():0;}bool resumeDsp(){return awaitingDsp()&&dsp_->resumeDsp();}bool stackWrites()const{if(dsp_)return dsp_->stackWrites();if(voice_)return false;auto s=task_.stage();return s==S::VoiceAddress||s==S::VoiceAddressAgain||s==S::SendCommand;}
 unsigned nextClocks(int previous=-1)const{if(dsp_)return dsp_->nextClocks();if(voice_){V::Credits c{3,7};return voice_->nextClocks(c);}auto s=task_.stage();switch(s){case S::AwaitChild:case S::Complete:return 0;case S::VoiceAddress:case S::VoiceAddressAgain:case S::SendCommand:return 10;case S::BindCommand:return 6;case S::ReadVoiceNote:case S::ReadVoiceNoteAgain:return 12+bus(voiceAddress_+13,-1);case S::ReadTranspose:return 12+bus(patch_+4,-1);case S::WriteMagnitude:return 12+bus(voiceAddress_+7,previous);case S::WriteCalculated:return 9+bus(0xd02f,previous);default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;if(dsp_){auto used=dsp_->advanceBefore(cost,read);if(dsp_->complete()){dsp_.reset();task_.resume(T::Child::SendCommand);}return used;}if(voice_){V::Credits c{3,7};auto used=voice_->advanceBefore(cost,c,[&](){return read(0xd023,1,cost);});if(voice_->complete()){voiceAddress_=voice_->address();voice_.reset();task_.resume(T::Child::VoiceAddress);}return used;}auto s=task_.stage();bool used=task_.advance([&](T::Read f){auto a=f==T::Read::Transpose?patch_+4:voiceAddress_+13;auto v=uint8_t(read(a&0xffffff,1,cost));if(s==S::ReadVoiceNote)firstNote_=v;return v;},[&](T::Write f,uint8_t v){auto a=f==T::Write::Magnitude?voiceAddress_+7:0xd02f;write(a&0xffffff,v,1,cost);});if(!used)return 0;if(task_.awaitingChild()){if(task_.child()==T::Child::VoiceAddress)voice_.emplace();else if(task_.child()==T::Child::SendCommand)dsp_.emplace(0x67,uint32_t(uint8_t(firstNote_+0xc4))<<16);}return cost;}
private:static unsigned bus(uint32_t a,int p){a&=0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0))return (p&1)||a!=uint32_t(p|1)?(a<0x400000?3:1):0;return a>=0xffffa0?1:3;}uint32_t patch_,voiceAddress_=0;uint8_t firstNote_=0;T task_{};std::optional<V>voice_{};std::optional<ControllerVoiceDspWordTask>dsp_{};
};
}
