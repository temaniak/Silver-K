#pragma once
#include "kstationMainLoopTask.h"
#include <cstdint>
namespace kstation {
// IDs are semantic board requests. Firmware pointers are resolved separately
// from verified metadata at the moment the live slot value is read.
enum class ControllerCallback:uint8_t{Unknown,Callback0,Callback1,Callback2,Callback3,Callback4,Callback5,Callback6,Callback7,Callback8,Callback9,Callback10};
class ControllerDeferredTask final {
public:
 enum class Phase:uint8_t{SaveDelay,ReadDelay,SelectEnabled,SaveTick,RequestTick,ReadStart,Elapsed,Compare,SelectDue,ClearValue,ClearDelay,ReadCallback,RequestCallback,RestoreTick,RestoreDelay,Return,AwaitTick,AwaitCallback,Complete};
 explicit ControllerDeferredTask(unsigned slot):slot_(slot),phase_(slot<5?Phase::SaveDelay:Phase::Complete){}
 unsigned slot()const{return slot_;}Phase phase()const{return phase_;}bool complete()const{return phase_==Phase::Complete;}bool awaitingTick()const{return phase_==Phase::AwaitTick;}bool awaitingCallback()const{return phase_==Phase::AwaitCallback;}ControllerCallback callback()const{return callback_;}uint32_t delayAddress()const{return 0xcd44+8*slot_;}
 bool resumeTick(uint16_t tick){if(!awaitingTick())return false;tick_=tick;phase_=Phase::ReadStart;return true;}bool resumeCallback(ControllerCallback id){if(!awaitingCallback()||id!=callback_)return false;phase_=Phase::RestoreTick;return true;}
 bool stackWrites()const{return phase_==Phase::SaveDelay||phase_==Phase::SaveTick||phase_==Phase::RequestTick||phase_==Phase::RequestCallback;}
 unsigned nextClocks()const{switch(phase_){case Phase::SaveDelay:case Phase::RestoreDelay:return 6;case Phase::SaveTick:case Phase::RequestTick:case Phase::RestoreTick:return 10;case Phase::ReadDelay:case Phase::ReadStart:case Phase::ClearDelay:return 12;case Phase::ReadCallback:return 18;case Phase::RequestCallback:return callback_==ControllerCallback::Unknown?0:5;case Phase::Return:return 7;case Phase::AwaitTick:case Phase::AwaitCallback:case Phase::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Resolver>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,Resolver&&resolve){auto cost=nextClocks();if(!cost||cost>available)return 0;switch(phase_){case Phase::ReadDelay:delay_=uint16_t(read(delayAddress(),2,cost));next();break;case Phase::SelectEnabled:phase_=delay_?Phase::SaveTick:Phase::RestoreDelay;break;case Phase::RequestTick:phase_=Phase::AwaitTick;break;case Phase::ReadStart:start_=uint16_t(read(delayAddress()-2,2,cost));next();break;case Phase::Elapsed:elapsed_=uint16_t(tick_-start_);next();break;case Phase::SelectDue:phase_=elapsed_<delay_?Phase::RestoreTick:Phase::ClearValue;break;case Phase::ClearDelay:write(delayAddress(),0,2,cost);next();break;case Phase::ReadCallback:callback_=resolve(read(delayAddress()+2,4,cost));next();break;case Phase::RequestCallback:phase_=Phase::AwaitCallback;break;case Phase::Return:phase_=Phase::Complete;break;case Phase::AwaitTick:case Phase::AwaitCallback:case Phase::Complete:break;default:next();break;}return cost;}
private:void next(){phase_=Phase(unsigned(phase_)+1);}unsigned slot_;Phase phase_;ControllerCallback callback_=ControllerCallback::Unknown;uint16_t delay_=0,start_=0,tick_=0,elapsed_=0;
};
class ControllerForegroundTickTask final {
public:
 enum class Phase:uint8_t{Bind,Read,Return,Complete};Phase phase()const{return phase_;}bool complete()const{return phase_==Phase::Complete;}uint16_t result()const{return uint16_t(value_);}unsigned nextClocks()const{switch(phase_){case Phase::Bind:return 9;case Phase::Read:return 8;case Phase::Return:return 7;case Phase::Complete:return 0;}return 0;}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&&read){auto cost=nextClocks();if(!cost||cost>available)return 0;if(phase_==Phase::Read)value_=read(0xfffd20,4,cost);phase_=Phase(unsigned(phase_)+1);return cost;}
private:Phase phase_=Phase::Bind;uint32_t value_=0;
};
}
