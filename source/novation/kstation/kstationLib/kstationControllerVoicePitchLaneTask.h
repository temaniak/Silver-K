#pragma once
#include "kstationVoiceUpdateTask.h"
#include "kstationVoiceAllocationTask.h"
#include <optional>
namespace kstation {
class ControllerVoicePitchLaneTask final {
 using T=VoiceParameterPitchLaneTask;using S=T::Stage;using V=VoiceAddressTask;
public:
 ControllerVoicePitchLaneTask(uint32_t patch,unsigned lane=0):task_(patch,lane){}bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.child()==T::Child::Write;}uint32_t word()const{return task_.word();}bool resumeDsp(){return awaitingDsp()&&task_.resumeChild();}bool stackWrites()const{if(voice_)return false;auto s=task_.stage();return s==S::DispatchVoice||s==S::DispatchWord;}
 unsigned nextClocks()const{if(voice_){V::Credits c{3,7};return voice_->nextClocks(c);}uint32_t a=0;unsigned width=0;access(a,width);T::Credits c{width?bus(a,width):0,task_.stage()==S::Return?7u:10u};return task_.nextClocks(c);}
 template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;if(voice_){V::Credits c{3,7};auto used=voice_->advanceBefore(cost,c,[&](){return read(0xd023,1,cost);});if(voice_->complete()){auto a=voice_->address();voice_.reset();task_.resumeChild(a);}return used;}uint32_t a=0;unsigned width=0;access(a,width);T::Credits c{width?bus(a,width):0,task_.stage()==S::Return?7u:10u};auto used=task_.advanceBefore(cost,c,[&](T::Read){return read(a&0xffffff,width,cost);});if(task_.child()==T::Child::Voice)voice_.emplace();return used;}
private:
 static unsigned bus(uint32_t a,unsigned n){int p=-1;unsigned c=0;for(unsigned i=0;i<n;++i){auto x=(a+i)&0xffffff;if(x<0x400000||(x>=0xfffd10&&x<0xffffa0)){if((p&1)||x!=uint32_t(p|1))c+=x<0x400000?3:1;p=int(x);}else{c+=x>=0xffffa0?1:3;p=-1;}}return c;}
 void access(uint32_t&a,unsigned&w)const{switch(task_.stage()){case S::ReadBase:a=0xbf9a;w=2;break;case S::ReadTune:a=task_.tuneAddress();w=1;break;case S::ReadCoarse:a=task_.coarseAddress();w=1;break;case S::ReadKey:a=task_.patch()+0xc2;w=1;break;case S::ReadFine:a=task_.fineAddress();w=1;break;case S::ReadOffset:a=0xbfc5;w=1;break;case S::ReadPoly:a=task_.patch()+0x81;w=1;break;case S::ReadUnit:a=task_.voice()+12;w=1;break;case S::ReadVoicePointer:a=task_.voice();w=4;break;case S::ReadDepth:a=task_.pointer()+1;w=1;break;case S::ReadAmount:a=task_.amountAddress();w=1;break;case S::ReadWheel:a=0xbfbe;w=2;break;default:break;}}
 T task_;std::optional<V>voice_{};
};
}
