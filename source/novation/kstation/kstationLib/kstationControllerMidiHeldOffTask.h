#pragma once
#include "kstationMidiEventTask.h"
#include <optional>
namespace kstation {
class ControllerMidiHeldOffTask final {
 using T=MidiHeldOffTask;using S=T::Stage;using O=MidiRemoveOrderTask;using OS=O::Stage;
public:
 explicit ControllerMidiHeldOffTask(uint8_t note):note_(note),task_(note){}bool complete()const{return task_.complete();}
 bool stackWrites()const{if(order_)return order_->stage()==OS::SaveWord;auto s=task_.stage();return s==S::SaveWord||s==S::SaveLong||s==S::RemoveOrder;}
 unsigned nextClocks()const{if(order_){switch(order_->stage()){case OS::Complete:return 0;case OS::ReadCount:case OS::ReadCountAgain:case OS::WriteCount:case OS::WriteCountMirror:return 12;case OS::BindList:return 9;case OS::ReadCandidate:case OS::ReadShift:return 8;case OS::WriteShift:return 9;case OS::SaveWord:case OS::RestoreWord:return 6;case OS::Return:return 7;default:return 3;}}switch(task_.stage()){case S::Complete:case S::AwaitChild:return 0;case S::SaveWord:case S::RestoreWord:return 6;case S::SaveLong:case S::RestoreLong:return 10;case S::BindPhysical:case S::BindHeld:return 9;case S::ReadMode:return 12;case S::ClearPhysical:case S::ClearHeld:return 12;case S::RemoveOrder:return 5;case S::Return:return 7;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write){auto cost=nextClocks();if(!cost||budget<cost)return 0;if(order_){auto s=order_->stage();bool ok=order_->advance([&](O::Read f){return read(f==O::Read::Count?0xcb57:order_->address(),f==O::Read::Count?1:2,f==O::Read::Count?cost:cost-2);},[&](O::Write f,uint32_t v){uint32_t a=f==O::Write::Shift?order_->address()-4:f==O::Write::Count?0xcb57:0xcb56;write(a,v,f==O::Write::Shift?2:1,cost);});(void)s;if(!ok)return 0;if(order_->complete()){order_.reset();task_.resumeRemoveOrder();}return cost;}
 bool ok=task_.advance([&](T::Read f){return read(f==T::Read::Mode?0xcd26:task_.bitmapAddress(),1,f==T::Read::Mode?cost:cost-3);},[&](T::Write,uint32_t v){write(task_.bitmapAddress(),v,1,cost);});if(!ok)return 0;if(task_.awaitingChild())order_.emplace(note_);return cost;}
private:uint8_t note_;T task_;std::optional<O>order_{};
};
}
