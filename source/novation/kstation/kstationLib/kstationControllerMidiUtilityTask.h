#pragma once
#include "kstationMidiPumpTask.h"
#include <cstdint>
namespace kstation {
// Semantic reset and activity helpers, with explicit interrupt-mask actions.
class ControllerMidiUtilityTask final {
 using T=MidiUtilityTask;using S=T::Stage;using R=T::Read;using W=T::Write;
public:
 using Kind=T::Kind;explicit ControllerMidiUtilityTask(Kind k):task_(k){}bool complete()const{return task_.complete();}bool stackWrites()const{return stage()==S::SaveMask||stage()==S::SaveWord;}bool saveMask()const{return stage()==S::SaveMask;}bool maskInterrupts()const{return stage()==S::Mask;}bool restoreMask()const{return stage()==S::RestoreMask;}S stage()const{return task_.stage();}
 unsigned nextClocks()const{switch(stage()){case S::SaveMask:case S::RestoreMask:return 9;case S::SaveWord:case S::RestoreWord:return 6;case S::Return:return 7;case S::WriteProducer:case S::WriteConsumer:case S::WriteStatus:return task_.kind()==Kind::ResetExternal?10:12;case S::ReadPort:case S::WritePort:return 4;case S::WriteHold:return 10;case S::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write){auto cost=nextClocks();if(!cost||cost>available)return 0;bool ext=task_.kind()==Kind::ResetExternal;bool used=task_.advance([&](R){return uint32_t(read(0xffffd2,cost));},[&](W w,uint32_t v){uint32_t a=0;switch(w){case W::Producer:a=ext?0xfffd2c:0xc9b6;break;case W::Consumer:a=ext?0xfffd2e:0xc9b8;break;case W::Status:a=ext?0xfffe30:0xcaba;break;case W::Port:a=0xffffd2;break;case W::Hold:a=0xfffe8f;break;}if(w==W::Producer||w==W::Consumer){write(a,uint8_t(v>>8),cost);write(a+1,uint8_t(v),cost);}else write(a,uint8_t(v),cost);});return used?cost:0;}
private:T task_;
};
}
