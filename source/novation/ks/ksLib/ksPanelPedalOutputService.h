#pragma once
#include "ksMidiOutputTask.h"
#include "ksStateChannelTask.h"
#include "ksPartControlTasks.h"
#include "ksInternalMidiQueueTask.h"
#include "ksSerialForegroundService.h"
#include <array>
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
// Complete pedal packet and per-byte local/serial publication. Every byte reads
// the live output mode; queue and serial waits retain their actual typed owner.
class PanelPedalOutputService final {
public:
 enum class Kind {Pedal,Byte};
 enum class Component {Pedal,Channel,Part,Byte,Queue,Serial};
 struct Input {uint16_t event=0;};
 struct Effect {bool read=false,write=false;};
 struct ByteResult {uint16_t event=0,producer=0,consumer=0;uint8_t mode=0;bool queued=false,overflow=false;};
 struct Summary {Kind kind=Kind::Pedal;uint16_t input=0,event=0;uint8_t part=0,channel=0,bytes=0;std::array<ByteResult,3> output{};};
 explicit PanelPedalOutputService(Kind kind):PanelPedalOutputService(kind,Input{}){}
 PanelPedalOutputService(Kind kind,Input input):kind_(kind){summary_.kind=kind;summary_.input=input.event;summary_.event=input.event;
  if(kind==Kind::Pedal){pedal_.emplace(MidiOutputTask::Kind::Pedal,input.event,0,0);select(Component::Pedal);}else startByte(input.event,true);
 }
 Kind kind()const{return kind_;}Component component()const{return component_;}uint32_t components()const{return components_;}
 bool complete()const{return complete_;}bool critical()const{return serial_&&serial_->critical();}Effect effect()const{return effect_;}const Summary&summary()const{return summary_;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,MidiOutputTask>)return component_==Component::Pedal&&pedal_?&*pedal_:component_==Component::Byte&&byte_?&*byte_:nullptr;
  else if constexpr(std::is_same_v<T,StateChannelTask>)return component_==Component::Channel&&channel_?&*channel_:nullptr;
  else if constexpr(std::is_same_v<T,PartLookupTask>)return component_==Component::Part&&part_?&*part_:nullptr;
  else if constexpr(std::is_same_v<T,InternalMidiQueueTask>)return component_==Component::Queue&&queue_?&*queue_:nullptr;
  else if constexpr(std::is_same_v<T,SerialForegroundService>)return component_==Component::Serial&&serial_?&*serial_:nullptr;
  else return nullptr;
 }
 unsigned nextClocks(int previousWrite=-1)const{
  if(complete_)return 0;
  switch(component_){case Component::Pedal:return pedal_->nextClocks();case Component::Byte:return byte_->nextClocks();
  case Component::Channel:return channel_->nextClocks();case Component::Part:return part_->stage()==PartLookupTask::Stage::Complete?17:part_->nextClocks();
  case Component::Queue:return queue_->nextClocks(byteClocks(0x15b08+queue_->index(),previousWrite),byteClocks(0x15c08,previousWrite));
  case Component::Serial:return serial_->nextClocks();}return 0;
 }
 template<class R,class W>unsigned advanceBefore(unsigned available,R&&read,W&&write,int previousWrite=-1){
  const unsigned cost=nextClocks(previousWrite);if(!cost||available<cost)return 0;effect_={};
  const auto readWord=[&](uint32_t a){const auto hi=uint8_t(read(a,cost));return uint16_t((unsigned(hi)<<8)|uint8_t(read(a+1,cost)));};
  const auto writeWord=[&](uint32_t a,uint16_t v){write(a,uint8_t(v>>8),cost);write(a+1,uint8_t(v),cost);};
  switch(component_){
  case Component::Pedal:{using T=MidiOutputTask;const auto stage=pedal_->stage();pedal_->advanceBefore(cost,[](T::Read){return 0;},[](T::Write,uint8_t){});
   using S=T::Stage;effect_.write=stage==S::SavePedal||stage==S::SelectPedalChannel||stage==S::SendPedalStatus||stage==S::SendPedalController;effect_.read=stage==S::RestorePedal;
   if(pedal_->awaitingChild()){
    if(stage==S::SelectPedalChannel){channel_.emplace(StateChannelTask::Kind::SelectedChannel);select(Component::Channel);}
    else startByte(pedal_->event(),false);
   }else if(pedal_->complete())startByte(pedal_->event(),true);
   break;
  }
  case Component::Channel:{using T=StateChannelTask;const auto stage=channel_->stage();channel_->advanceBefore(cost,[&](T::Read f){return read(f==T::Read::SelectedPart?0x16798:0x174aeu+20*channel_->part()+11,cost);},[](T::Write,uint8_t){});
   using S=T::Stage;effect_.write=stage==S::AdmitContext||stage==S::LookupControls;effect_.read=stage==S::ReleaseContext||stage==S::ReturnSelected;
   if(channel_->awaitingControls()){part_.emplace(PartLookupTask::controlsFor(channel_->part()));select(Component::Part);}
   else if(channel_->complete()){summary_.part=channel_->part();summary_.channel=channel_->channel();pedal_->resumeChild(uint16_t((unsigned(summary_.part)<<8)|summary_.channel),0,false);channel_.reset();select(Component::Pedal);}break;
  }
  case Component::Part:
   if(part_->stage()==PartLookupTask::Stage::Complete){effect_.read=true;part_.reset();channel_->resumeControls();select(Component::Channel);}
   else part_->advanceBefore(cost,[](PartLookupTask::Layout){return 0;});break;
  case Component::Byte:{using T=MidiOutputTask;using S=T::Stage;const auto stage=byte_->stage();byte_->advanceBefore(cost,[&](T::Read){return read(0x169ed,cost);},[](T::Write,uint8_t){});
   effect_.write=stage==S::SaveByteContext||stage==S::QueueByte;effect_.read=stage==S::RestoreByteContext;
   if(stage==S::ReadByteMode)current_.mode=byte_->mode();
   if(byte_->awaitingChild()){current_.queued=true;queue_.emplace(InternalMidiQueueTask::Kind::Push,byte_->low());select(Component::Queue);}
   else if(byte_->complete()){serial_.emplace(SerialForegroundService::Kind::Transmit,byte_->low());select(Component::Serial);}break;
  }
  case Component::Queue:{using T=InternalMidiQueueTask;using S=T::Stage;const auto stage=queue_->stage();queue_->advanceBefore(cost,[&](T::Read f){const auto v=readWord(f==T::Read::Producer?0x15b04:0x15b06);if(v>255)throw std::logic_error("Pedal live internal queue index invalid");return v;},[&](T::Write f,uint16_t v){if(f==T::Write::Byte||f==T::Write::Overflow)write(f==T::Write::Byte?0x15b08+queue_->index():0x15c08,uint8_t(v),cost);else writeWord(f==T::Write::Producer?0x15b04:0x15b06,v);},byteClocks(0x15b08+queue_->index(),previousWrite),byteClocks(0x15c08,previousWrite));
   effect_.write=stage==S::SavePush||stage==S::SavePushAddress;effect_.read=stage==S::RestorePush||stage==S::RestorePushAddress||stage==S::ReturnPush;
   if(queue_->complete()){current_.overflow=queue_->full();queue_.reset();byte_->resumeChild(byte_->event(),0,false);select(Component::Byte);}break;
  }
  case Component::Serial:{using S=SerialTransmitTask::Stage;const bool retained=serial_->atTransmit(S::RetainFirst)||serial_->atTransmit(S::RetainSecond)||serial_->atTransmit(S::RetainCritical);const bool released=serial_->atTransmit(S::RestoreCritical)||serial_->atTransmit(S::RestoreSecond)||serial_->atTransmit(S::RestoreFirst)||serial_->atTransmit(S::Finish);
   serial_->advanceBefore(cost,read,write);effect_={released,retained};
   if(serial_->producer()>255||serial_->consumer()>255)throw std::logic_error("Pedal live serial queue index invalid");
   if(serial_->complete()){current_.producer=serial_->producer();current_.consumer=serial_->consumer();if(summary_.bytes>=summary_.output.size())throw std::logic_error("Pedal byte outcome capacity exceeded");summary_.output[summary_.bytes++]=current_;summary_.event=current_.event;serial_.reset();byte_.reset();
    if(tail_)complete_=true;else{pedal_->resumeChild(current_.event,0,false);select(Component::Pedal);}}
   break;
  }
  }return cost;
 }
private:
 static unsigned byteClocks(uint32_t a,int previous){return !(previous&1)&&a==(uint32_t(previous)|1u)?0:3;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void startByte(uint16_t event,bool tail){current_={};current_.event=event;tail_=tail;byte_.emplace(MidiOutputTask::Kind::Byte,event,0,0);select(Component::Byte);}
 Kind kind_;Component component_=Component::Pedal;uint32_t components_=0;bool complete_=false,tail_=false;Effect effect_{};Summary summary_{};ByteResult current_{};
 std::optional<MidiOutputTask>pedal_,byte_;std::optional<StateChannelTask>channel_;std::optional<PartLookupTask>part_;std::optional<InternalMidiQueueTask>queue_;std::optional<SerialForegroundService>serial_;
};
}
