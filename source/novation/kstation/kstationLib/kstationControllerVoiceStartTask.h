#pragma once
#include "kstationMidiEventTask.h"
#include "kstationVoiceAllocationTask.h"
#include <optional>
namespace kstation {
class ControllerVoiceStartTask final {
 using T=MidiVoiceStartPrefixTask;using S=T::Stage;using P=VoicePartAddressTask;using PS=P::Stage;
public:
 ControllerVoiceStartTask(uint8_t part,uint8_t note,uint8_t velocity):task_(part,note,velocity){}bool complete()const{return task_.complete();}bool awaitingBuild()const{return task_.awaitingChild()&&!part_&&(task_.child()==T::Child::BuildVoice||task_.child()==T::Child::TailBuildVoice);}bool tailBuild()const{return awaitingBuild()&&task_.child()==T::Child::TailBuildVoice;}uint32_t patch()const{return task_.patch();}uint32_t partAddress()const{return task_.partAddress();}bool resumeBuild(){return awaitingBuild()&&task_.resume(task_.child());}
 bool stackWrites()const{if(part_)return false;auto s=task_.stage();return s==S::LookupFirst||s==S::LookupAgain||s==S::SavePart||s==S::BuildVoice;}
 unsigned nextClocks(int lastWrite=-1)const{if(part_){P::Credits cr{3,7};return part_->nextClocks(cr);}auto s=task_.stage();switch(s){case S::Complete:case S::AwaitChild:return 0;case S::WritePart:return 9+bus(0xd028,1,lastWrite);case S::WriteNote:return 9+bus(0xd02a,1,lastWrite);case S::WriteVelocity:return 9+bus(0xd02b,1,lastWrite);case S::LookupFirst:case S::LookupAgain:case S::BuildVoice:return 5;case S::ReadPatch:return 15+bus(task_.partAddress(),4,-1);case S::ReadUnison:return 12+bus(task_.patch()+0x81,1,-1);case S::ReadCounter:case S::ReadNext:return 12+bus(task_.partAddress()+15,1,-1);case S::WriteCounter:case S::WriteNext:return 12+bus(task_.partAddress()+15,1,lastWrite);case S::SavePart:case S::RestorePart:return 10;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int lastWrite=-1){auto cost=nextClocks(lastWrite);if(!cost||cost>budget)return 0;if(part_){P::Credits cr{3,7};auto used=part_->advanceBefore(cost,cr,[&](){return read(0xd028,1,cost);});if(part_->complete()){auto address=part_->address();part_.reset();task_.resume(T::Child::LookupPart,address);}return used;}auto stage=task_.stage();bool ok=task_.advance([&](T::Read f)->uint32_t{switch(f){case T::Read::Patch:return read(task_.partAddress(),4,cost);case T::Read::Unison:return read((task_.patch()+0x81)&0xffffff,1,cost);case T::Read::Counter:case T::Read::Next:return read((task_.partAddress()+15)&0xffffff,1,cost);}return 0;},[&](T::Write f,uint32_t v){uint32_t a=f==T::Write::Part?0xd028:f==T::Write::Note?0xd02a:f==T::Write::Velocity?0xd02b:(task_.partAddress()+15)&0xffffff;write(a,v,1,cost);});(void)stage;if(!ok)return 0;if(task_.awaitingChild()&&task_.child()==T::Child::LookupPart)part_.emplace();return cost;}
private:
 static unsigned bus(uint32_t addr,unsigned n,int prev){unsigned cost=0;for(unsigned i=0;i<n;++i){auto a=(addr+i)&0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0)){if((prev&1)||a!=uint32_t(prev|1))cost+=a<0x400000?3:1;prev=int(a);}else{cost+=a>=0xffffa0?1:3;prev=-1;}}return cost;}
 T task_;std::optional<P>part_{};
};
}
