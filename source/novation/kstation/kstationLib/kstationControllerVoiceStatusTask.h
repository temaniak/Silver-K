#pragma once
#include "kstationProgramVoicesTask.h"
namespace kstation {
class ControllerVoiceStatusTask final {
 using T=ProgramVoiceStatusTask;using S=T::Stage;
public:
 bool complete()const{return task_.complete();}bool awaitingDsp()const{return task_.awaiting();}uint16_t address()const{return 0x802d;}uint32_t word()const{return slot_;}bool resumeDsp(){if(!awaitingDsp())return false;task_.resume();return true;}bool stackWrites()const{return task_.stage()==S::Send;}
 unsigned nextClocks()const{FlashCredits c{};c.read=3;c.write=3;return task_.nextClocks(c);}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write){auto cost=nextClocks();if(!cost||cost>budget)return 0;FlashCredits c{};c.read=3;c.write=3;return task_.advanceBefore(cost,c,[&](){slot_=uint8_t(read(0xd023,1,cost));return slot_;},[&](unsigned value){write(0xd0f0,value,1,cost);});}
private:T task_{};uint8_t slot_=0;
};
}
