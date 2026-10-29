#pragma once
#include "kstationMidiEventTask.h"
namespace kstation {
class ControllerMidiDspTriggerTask final {
 using T=MidiDspTriggerTask;using S=T::Stage;
public:
 using Kind=T::Kind;explicit ControllerMidiDspTriggerTask(Kind k):task_(k),kind_(k){}bool complete()const{return task_.complete();}bool awaitingTail()const{return task_.awaitingChild();}bool resumeTail(){return task_.resumeTail();}uint8_t triggerValue()const{return kind_==Kind::Reset?0:1;}
 bool stackWrites()const{return task_.stage()==S::SaveWord;}
 unsigned nextClocks()const{switch(task_.stage()){case S::Complete:case S::AwaitChild:return 0;case S::SaveWord:case S::RestoreWord:return 6;case S::ReadPending:case S::ClearPending:return 12;case S::Return:return 7;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write){auto cost=nextClocks();if(!cost||cost>budget)return 0;return task_.advance([&](T::Read){return read(0xd0ee,1,cost);},[&](T::Write,uint8_t v){write(0xd0ee,v,1,cost);})?cost:0;}
private:T task_;Kind kind_;
};
}
