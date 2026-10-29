#pragma once
#include "kstationVoiceAllocationTask.h"
#include <optional>
namespace kstation {
class ControllerVoiceSelectionTask final {
 using T=VoiceSelectionTask;using S=T::Stage;using V=VoiceAddressTask;using P=VoicePartAddressTask;
public:
 explicit ControllerVoiceSelectionTask(uint32_t patch):task_(patch){}bool complete()const{return task_.complete();}uint8_t slot()const{return slot_;}uint8_t allocation()const{return allocation_;}
 bool stackWrites()const{if(part_||voice_)return false;auto s=task_.stage();return s==S::DispatchPart||s==S::DispatchOwnerVoice||s==S::DispatchNewestVoice||s==S::DispatchFreeVoice||s==S::DispatchNoteVoice;}
 unsigned nextClocks(int lastWrite=-1)const{if(part_){P::Credits c{3,7};return part_->nextClocks(c);}if(voice_){V::Credits c{3,7};return voice_->nextClocks(c);}uint32_t ra=0,wa=0;unsigned rn=0,wn=0;access(ra,rn,wa,wn);T::Credits c{rn?bus(ra,rn,-1):0,wn?bus(wa,wn,lastWrite):0,10};return task_.nextClocks(c);}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int lastWrite=-1){auto cost=nextClocks(lastWrite);if(!cost||cost>budget)return 0;if(part_){P::Credits c{3,7};auto used=part_->advanceBefore(cost,c,[&](){return read(0xd028,1,cost);});if(part_->complete()){auto v=part_->address();part_.reset();task_.resumeChild(v);}return used;}if(voice_){V::Credits c{3,7};auto used=voice_->advanceBefore(cost,c,[&](){return read(0xd023,1,cost);});if(voice_->complete()){auto v=voice_->address();voice_.reset();task_.resumeChild(v);}return used;}uint32_t ra=0,wa=0;unsigned rn=0,wn=0;access(ra,rn,wa,wn);T::Credits c{rn?bus(ra,rn,-1):0,wn?bus(wa,wn,lastWrite):0,10};auto used=task_.advanceBefore(cost,c,[&](T::Read f){auto v=read(ra&0xffffff,rn,cost);if(f==T::Read::Index)slot_=uint8_t(v);return v;},[&](T::Write f,uint8_t v){if(f==T::Write::Index)slot_=v;else allocation_=v;write(wa&0xffffff,v,wn,cost); });if(task_.child()==T::Child::Part)part_.emplace();else if(task_.child()==T::Child::Voice)voice_.emplace();return used;}
private:
 static unsigned bus(uint32_t a,unsigned n,int previous){unsigned c=0;for(unsigned i=0;i<n;++i){auto x=(a+i)&0xffffff;if(x<0x400000||(x>=0xfffd10&&x<0xffffa0)){if((previous&1)||x!=uint32_t(previous|1))c+=x<0x400000?3:1;previous=int(x);}else{c+=x>=0xffffa0?1:3;previous=-1;}}return c;}
 void access(uint32_t&ra,unsigned&rn,uint32_t&wa,unsigned&wn)const{switch(task_.stage()){
 case S::ReadMode:ra=task_.patchAddress()+0x80;rn=1;break;case S::ReadHeld:ra=task_.partAddress()+14;rn=1;break;case S::ReadUnit:ra=task_.partAddress()+15;rn=1;break;case S::ReadPart:ra=0xd028;rn=1;break;case S::ReadCount:ra=0xd022;rn=1;break;
 case S::ReadOwnerAge:case S::ReadNewestAge:case S::ReadFreeAge:case S::ReadNoteAge:ra=task_.voiceAddress()+8;rn=4;break;case S::ReadOwnerPart:case S::ReadNewestPart:case S::ReadNotePart:ra=task_.voiceAddress()+4;rn=1;break;case S::ReadOwnerUnit:case S::ReadNewestUnit:case S::ReadNoteUnit:ra=task_.voiceAddress()+12;rn=1;break;
 case S::ReadOldestIndex:case S::ReadOwnerIndex:case S::ReadNewestIndex:case S::ReadNewestNext:case S::ReadFreeIndex:case S::ReadFreeNext:case S::ReadNoteIndex:case S::ReadNoteNext:ra=0xd023;rn=1;break;case S::ReadIncoming:ra=0xd02a;rn=1;break;case S::ReadNote:ra=task_.voiceAddress()+5;rn=1;break;
 case S::PublishInitial:case S::PublishOwnerIndex:case S::PublishOwnerCandidate:case S::PublishNewestIndex:case S::PublishNewestCandidate:case S::PublishFreeIndex:case S::PublishFreeCandidate:case S::PublishNoteIndex:case S::PublishNoteCandidate:wa=0xd023;wn=1;break;
 case S::PublishReuse:case S::PublishOwnerAllocation:case S::PublishNewestAllocation:case S::PublishPolyAllocation:case S::PublishNoteAllocation:case S::PublishRetrigger:wa=0xd02c;wn=1;break;default:break;}}
 T task_;uint8_t slot_=0,allocation_=0;std::optional<P>part_{};std::optional<V>voice_{};
};
}
