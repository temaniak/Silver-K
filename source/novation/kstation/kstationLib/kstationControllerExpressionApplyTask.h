#pragma once
#include "kstationMidiEventTask.h"
#include "kstationVoiceAllocationTask.h"
#include <optional>
namespace kstation {
class ControllerExpressionApplyTask final {
 using T=MidiApplyExpressionTask;using S=T::Stage;using P=VoicePartAddressTask;
public:
 using Kind=T::Kind;using Child=T::Child;ControllerExpressionApplyTask(Kind kind,uint8_t value):task_(kind,0,value){}bool complete()const{return task_.complete();}bool awaitingChild()const{return task_.awaitingChild()&&!part_;}Child child()const{return task_.child();}uint32_t partAddress()const{return task_.partAddress();}bool resume(Child c){return awaitingChild()&&task_.resume(c);}
 bool stackWrites()const{if(part_)return false;auto s=task_.stage();return s==S::LookupPart||s==S::UpdatePitch||s==S::UpdatePressure||s==S::UpdatePressureTail;}
 unsigned nextClocks(int previous=-1)const{if(part_){P::Credits c{3,7};return part_->nextClocks(c);}switch(task_.stage()){case S::Complete:case S::AwaitChild:return 0;case S::WritePart:return 12;case S::LookupPart:case S::UpdatePitch:case S::UpdatePressure:case S::UpdatePressureTail:return 10;case S::ExpandMaximum:return 6;case S::MultiplyPressure:return 15;case S::WriteValue:return 12+bus(task_.partAddress()+(task_.kind()==Kind::Pitch?4:7),task_.kind()==Kind::Pitch?2:1,previous);case S::ReadInhibit:return 12+bus(task_.partAddress()+14,1,-1);case S::Return:return 7;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;if(part_){P::Credits c{3,7};auto used=part_->advanceBefore(cost,c,[&](){return read(0xd028,1,cost);});if(part_->complete()){auto a=part_->address();part_.reset();task_.resume(Child::LookupPart,a);}return used;}bool ok=task_.advance([&](T::Read){return read((task_.partAddress()+14)&0xffffff,1,cost);},[&](T::Write f,uint32_t v){auto a=f==T::Write::Part?0xd028:task_.partAddress()+(f==T::Write::Pitch?4:7);write(a&0xffffff,v,f==T::Write::Pitch?2:1,cost);});if(!ok)return 0;if(task_.awaitingChild()&&task_.child()==Child::LookupPart)part_.emplace();return cost;}
private:static unsigned bus(uint32_t a,unsigned n,int p){unsigned c=0;for(unsigned i=0;i<n;++i){auto x=(a+i)&0xffffff;if(x<0x400000||(x>=0xfffd10&&x<0xffffa0)){if((p&1)||x!=uint32_t(p|1))c+=x<0x400000?3:1;p=int(x);}else{c+=x>=0xffffa0?1:3;p=-1;}}return c;}T task_;std::optional<P>part_{};
};
}
