#pragma once
#include "kstationMidiPumpTask.h"
#include <cstdint>
namespace kstation {
// Live consumer-side MIDI ring service. Mask transitions are semantic actions
// for the shared IRQ scheduler and never expose architectural flags.
class ControllerMidiDequeueTask final {
 using T=MidiDequeueTask;using S=T::Stage;using R=T::Read;using W=T::Write;
public:
 using Kind=T::Kind;explicit ControllerMidiDequeueTask(Kind k):task_(k){}S stage()const{return task_.stage();}bool complete()const{return task_.complete();}uint16_t result()const{return task_.result();}bool stackWrites()const{return stage()==S::SaveFirst||stage()==S::SaveSecond;}bool saveMask()const{return stage()==S::SaveMask;}bool maskInterrupts()const{return stage()==S::Mask;}bool restoreMask()const{return stage()==S::RestoreMask;}
 unsigned nextClocks()const{switch(stage()){case S::SaveFirst:case S::SaveSecond:case S::RestoreSecond:case S::RestoreFirst:return 10;case S::Return:return 7;case S::LocateStatus:case S::LocateData:return 9;case S::MaskIndex:return 6;case S::ReadStatus:case S::ClearStatus:return 4;case S::ReadConsumer:case S::ReadProducer:case S::WriteConsumer:return task_.kind()==Kind::External?10:12;case S::ReadData:{auto a=task_.dataAddress()&0xffffff;return 3+(a<0x400000||a<0xfffd10?3:1);}case S::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write){auto cost=nextClocks();if(!cost||cost>available)return 0;bool external=task_.kind()==Kind::External;auto consumer=external?0xfffd2e:0xc9b8,producer=external?0xfffd2c:0xc9b6;bool used=task_.advance([&](R r)->uint32_t{switch(r){case R::Status:return read(0xfffe30,cost);case R::Consumer:{auto hi=read(consumer,cost);auto lo=read(consumer+1,cost);return (uint16_t(hi)<<8)|uint8_t(lo);}case R::Producer:{auto hi=read(producer,cost);auto lo=read(producer+1,cost);return (uint16_t(hi)<<8)|uint8_t(lo);}case R::Data:return read(task_.dataAddress()&0xffffff,cost);}return 0;},[&](W w,uint32_t v){if(w==W::Status)write(0xfffe30,uint8_t(v),cost);else{write(consumer,uint8_t(v>>8),cost);write(consumer+1,uint8_t(v),cost);}});return used?cost:0;}
private:T task_;
};
}
