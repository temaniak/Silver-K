#pragma once
#include "kstationMidiEventTask.h"
#include "kstationVoiceAllocationTask.h"
#include "kstationControllerVoiceDspWordTask.h"
#include <optional>
namespace kstation {
class ControllerVoiceLongPortamento final {
 using T=MidiVoicePortamentoTask;using S=T::Stage;using V=VoiceAddressTask;
public:
 ControllerVoiceLongPortamento(uint32_t patch,uint32_t input):patch_(patch),task_(input){}bool complete()const{return task_.awaitingChild()&&task_.child()==T::Child::ContinueLong;}bool awaitingDsp()const{return dsp_&&dsp_->awaitingDsp();}uint16_t address()const{return dsp_?dsp_->address():0;}uint32_t word()const{return dsp_?dsp_->word():0;}bool resumeDsp(){return awaitingDsp()&&dsp_->resumeDsp();}bool stackWrites()const{if(dsp_)return dsp_->stackWrites();if(voice_)return false;auto s=task_.stage();return s==S::VoiceAddress||s==S::SaveOriginal||s==S::SendFirst||s==S::SendSecond||s==S::SendAlternateFirst||s==S::SendAlternateSecond;}
 unsigned nextClocks()const{if(dsp_)return dsp_->nextClocks();if(voice_){V::Credits c{3,7};return voice_->nextClocks(c);}switch(task_.stage()){case S::AwaitChild:case S::Complete:return 0;case S::ReadFlags:return 12+bus(patch_+5);case S::ReadGlide:return 12+bus(voiceAddress_+7);case S::VoiceAddress:case S::SendFirst:case S::SendSecond:case S::SendAlternateFirst:case S::SendAlternateSecond:case S::SaveOriginal:case S::RestoreOriginal:return 10;case S::MulHigh:case S::MulLow:return 23;case S::CompareMinimum:return 9;case S::SetMinimum:case S::BindFirst:case S::BindSecond:case S::BindAlternateFirst:case S::BindAlternateSecond:return 6;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;if(dsp_){auto used=dsp_->advanceBefore(cost,read);if(dsp_->complete()){auto child=task_.child();dsp_.reset();task_.resume(child);}return used;}if(voice_){V::Credits c{3,7};auto used=voice_->advanceBefore(cost,c,[&](){return read(0xd023,1,cost);});if(voice_->complete()){voiceAddress_=voice_->address();voice_.reset();task_.resume(T::Child::VoiceAddress);}return used;}bool used=task_.advance([&](T::Read f){return uint8_t(read((f==T::Read::Flags?patch_+5:voiceAddress_+7)&0xffffff,1,cost));});if(!used)return 0;if(task_.awaitingChild()){if(task_.child()==T::Child::VoiceAddress)voice_.emplace();else if(task_.child()==T::Child::SendFirst||task_.child()==T::Child::SendSecond)dsp_.emplace(task_.child()==T::Child::SendFirst?0x68:0x69,task_.word());}return cost;}
private:static unsigned bus(uint32_t a){return (a&0xffffff)<0xfffd10?3:1;}uint32_t patch_,voiceAddress_=0;T task_;std::optional<V>voice_{};std::optional<ControllerVoiceDspWordTask>dsp_{};
};
}
