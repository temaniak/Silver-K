#pragma once
#include "ksInternalMidiQueueTask.h"
#include <stdexcept>
#include <type_traits>
namespace ks {
class NativeMidiQueueService final {
public:
 using Kind=InternalMidiQueueTask::Kind;using Stage=InternalMidiQueueTask::Stage;struct Effect{bool read=false,write=false;};
 explicit NativeMidiQueueService(Kind k,uint8_t byte=0):kind_(k),queue_(k,byte){}
 bool complete()const{return queue_.complete();}bool critical()const{return false;}unsigned priority()const{return 0;}Kind kind()const{return kind_;}const Effect&effect()const{return effect_;}const InternalMidiQueueTask&queueTask()const{return queue_;}uint16_t result()const{return uint16_t((queue_.available()?0x100:0)|queue_.value());}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,InternalMidiQueueTask>)return complete()?nullptr:&queue_;else return nullptr;}
 unsigned nextClocks(int previous=-1)const{return queue_.nextClocks(byteClock(0x15b08u+queue_.index(),previous),byteClock(0x15c08,previous));}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;const auto stage=queue_.stage();effect_={};queue_.advanceBefore(cost,[&](InternalMidiQueueTask::Read r)->uint16_t{if(r==InternalMidiQueueTask::Read::Byte)return uint8_t(read(0x15b08u+queue_.index(),cost));const auto a=r==InternalMidiQueueTask::Read::Consumer?0x15b06u:0x15b04u;return uint16_t((unsigned(uint8_t(read(a,cost)))<<8)|uint8_t(read(a+1,cost)));},[&](InternalMidiQueueTask::Write w,uint16_t v){if(w==InternalMidiQueueTask::Write::Byte||w==InternalMidiQueueTask::Write::Overflow)write(w==InternalMidiQueueTask::Write::Byte?0x15b08u+queue_.index():0x15c08u,uint8_t(v),cost);else{const auto a=w==InternalMidiQueueTask::Write::Consumer?0x15b06u:0x15b04u;write(a,uint8_t(v>>8),cost);write(a+1,uint8_t(v),cost);}},byteClock(0x15b08u+queue_.index(),previous),byteClock(0x15c08,previous));
 switch(stage){case Stage::SaveReset:case Stage::SavePop:case Stage::SavePopAddress:case Stage::SavePush:case Stage::SavePushAddress:effect_.write=true;break;case Stage::RestoreReset:case Stage::ReturnReset:case Stage::RestorePopAddress:case Stage::RestorePop:case Stage::ReturnPop:case Stage::RestorePushAddress:case Stage::RestorePush:case Stage::ReturnPush:effect_.read=true;break;default:break;}return cost;}
private:static unsigned byteClock(uint32_t a,int p){return a<0x400000&&!(p&1)&&a==(unsigned(p)|1u)?0:3;}Kind kind_;InternalMidiQueueTask queue_;Effect effect_{};
};
}
