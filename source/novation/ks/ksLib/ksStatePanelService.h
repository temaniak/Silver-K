#pragma once
#include "ksStateForegroundTask.h"
#include "ksKeyboardEventsTask.h"
#include "ksMidiOutputTask.h"
#include "ksStateChannelTask.h"
#include "ksPartControlTasks.h"
#include "ksInternalMidiQueueTask.h"
#include "ksSerialForegroundService.h"
#include <optional>
namespace ks {
// Complete upper-State keyboard/control drain. MIDI routing, local eligibility,
// internal packets and UART waits all return through typed owners, including
// the packet tails. No external handler or compatibility child is requested.
class StatePanelService final {
public:
 enum class Kind {Controls};
 enum class Component {Controls,Keyboard,Route,Packet,Local,Channel,Part,Queue,Serial};
 explicit StatePanelService(Kind kind=Kind::Controls):kind_(kind){}
 Kind kind()const{return kind_;}Component component()const{return active_;}uint32_t components()const{return components_;}
 bool complete()const{return controls_.complete();}bool critical()const{return serial_&&serial_->critical();}
 bool biasObserved()const{return biasObserved_;}uint8_t bias()const{return controls_.bias();}
 uint16_t event()const{return controls_.event();}uint32_t controlAddress()const{return 0xffeeaeu + controls_.index();}
 uint64_t consumed()const{return consumed_;}uint64_t routed()const{return routed_;}
 bool atKeyboard(KeyboardEventsTask::Stage s)const{return active_==Component::Keyboard&&keyboard_&&keyboard_->stage()==s;}
 bool atRoute(MidiOutputTask::Stage s)const{return active_==Component::Route&&route_&&route_->stage()==s;}
 bool atPacket(MidiOutputTask::Stage s)const{return active_==Component::Packet&&packet_&&packet_->stage()==s;}
 bool atLocal(MidiOutputTask::Stage s)const{return active_==Component::Local&&local_&&local_->stage()==s;}
 bool atQueue(InternalMidiQueueTask::Stage s)const{return active_==Component::Queue&&queue_&&queue_->stage()==s;}
 bool atSerial(SerialTransmitTask::Stage s)const{return active_==Component::Serial&&serial_&&serial_->atTransmit(s);}
 bool atControls(StateForegroundTask::Stage s)const{return active_==Component::Controls&&controls_.stage()==s;}
 bool clearsWriteHistory()const{
  using F=StateForegroundTask::Stage;using K=KeyboardEventsTask::Stage;using M=MidiOutputTask::Stage;using Q=InternalMidiQueueTask::Stage;using C=StateChannelTask::Stage;
  if(active_==Component::Controls){switch(controls_.stage()){case F::PopControl:case F::DispatchControlPress:case F::DispatchControlRelease:return true;default:return false;}}
  if(active_==Component::Keyboard)return keyboard_->stage()==K::SavePopValue||keyboard_->stage()==K::SavePopIndex;
  if(active_==Component::Channel)return channel_->stage()==C::AdmitContext||channel_->stage()==C::LookupControls;
  if(active_==Component::Queue)return queue_->stage()==Q::SavePush||queue_->stage()==Q::SavePushAddress;
  if(active_==Component::Serial)return serial_->atTransmit(SerialTransmitTask::Stage::RetainFirst)||serial_->atTransmit(SerialTransmitTask::Stage::RetainSecond)||serial_->atTransmit(SerialTransmitTask::Stage::RetainCritical);
  const auto*t=midiTask();if(!t)return false;switch(t->stage()){
  case M::SaveOffContext:case M::SaveOnContext:case M::SaveOffEvent:case M::SaveOnEvent:case M::QueueOffEvent:case M::QueueOnEvent:case M::CheckOffLocal:case M::CheckOnLocal:case M::SendOffEvent:case M::SendOnEvent:
  case M::SaveQueueOff:case M::SaveQueueOn:case M::SelectQueueOffChannel:case M::SelectQueueOnChannel:case M::QueueNoteStatus:case M::QueueNote:
  case M::SaveTransmitOff:case M::SaveTransmitOn:case M::SelectTransmitOffChannel:case M::SelectTransmitOnChannel:case M::TransmitNoteStatus:case M::TransmitNote:
  case M::SaveLocalEvent:case M::SelectLocalPart:case M::LocateLocalControls:return true;default:return false;}
 }
 unsigned nextClocks(int previousWrite=-1)const{
  if(complete())return 0;
  switch(active_){case Component::Controls:return controls_.nextClocks(3);
  case Component::Keyboard:return keyboard_->nextClocks(wordClocks(0x156ec+keyboard_->consumer()));
  case Component::Route:case Component::Packet:case Component::Local:return midiTask()->nextClocks(3,byteClocks(0x17526,previousWrite));
  case Component::Channel:return channel_->nextClocks();
  case Component::Part:return part_->stage()==PartLookupTask::Stage::Complete?17:part_->nextClocks();
  case Component::Queue:return queue_->nextClocks(byteClocks(0x15b08+queue_->index(),previousWrite),3);
  case Component::Serial:return serial_->nextClocks();}return 0;
 }
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;
  const auto readWord=[&](uint32_t a){const auto gap=wordClocks(a)-(a>=0xfffd10?1u:3u);const auto hi=uint8_t(read(a,cost-gap));return uint16_t((unsigned(hi)<<8)|uint8_t(read(a+1,cost)));};
  const auto writeWord=[&](uint32_t a,uint16_t v){const auto gap=wordClocks(a)-(a>=0xfffd10?1u:3u);write(a,uint8_t(v>>8),cost-gap);write(a+1,uint8_t(v),cost);};
  switch(active_){
  case Component::Controls:{using T=StateForegroundTask;const auto stage=controls_.stage();
   controls_.advanceBefore(cost,[&](T::Read f)->uint16_t{return read(f==T::Read::ControlBias?0x1678d:0xffeeae + controls_.index(),cost);},[&](T::Write,uint16_t v){write(0xffeeae + controls_.index(),uint8_t(v),cost);},3);
   if(stage==T::Stage::ReadControlBias)biasObserved_=true;
   if(controls_.awaitingChild()){
    if(stage==T::Stage::PopControl){keyboard_.emplace(KeyboardEventsTask::Kind::Pop);select(Component::Keyboard);}
    else {route_.emplace(stage==T::Stage::DispatchControlPress?MidiOutputTask::Kind::RouteOn:MidiOutputTask::Kind::RouteOff,controls_.event(),0,0);++routed_;select(Component::Route);}
   }break;
  }
  case Component::Keyboard:{using T=KeyboardEventsTask;
   keyboard_->advanceBefore(cost,[&](T::Read f){return readWord(f==T::Read::Producer?0x156e8:f==T::Read::Consumer?0x156ea:0x156ec+keyboard_->consumer());},[&](T::Write,uint16_t v){writeWord(0x156ea,v);},wordClocks(0x156ec+keyboard_->consumer()));
   if(keyboard_->complete()){const auto event=keyboard_->message();if(keyboard_->consumer()!=keyboard_->producer())++consumed_;keyboard_.reset();controls_.resumeChild(event,false);select(Component::Controls);}break;
  }
  case Component::Route:case Component::Packet:case Component::Local:{
   using T=MidiOutputTask;using S=T::Stage;const auto owner=active_;auto&t=*midiTask();const auto stage=t.stage();
   t.advanceBefore(cost,[&](T::Read f){return read(f==T::Read::SendMode?0x169ed:t.address()+327,cost);},[&](T::Write,uint8_t v){write(0x17526,v,cost);},3,byteClocks(0x17526,previousWrite));
   if(owner==Component::Local&&stage==S::ReadLocalFlags)localEnabled_=bool(t.flags()&4);
   if(t.awaitingChild())switch(stage){
   case S::QueueOnEvent:case S::QueueOffEvent:packet_.emplace(stage==S::QueueOnEvent?T::Kind::QueueOn:T::Kind::QueueOff,t.event(),0,0);select(Component::Packet);break;
   case S::CheckOnLocal:case S::CheckOffLocal:localEnabled_=false;local_.emplace(T::Kind::Local,t.event(),0,0);select(Component::Local);break;
   case S::SendOnEvent:case S::SendOffEvent:packet_.emplace(stage==S::SendOnEvent?T::Kind::TransmitOn:T::Kind::TransmitOff,t.event(),0,0);select(Component::Packet);break;
   case S::SelectQueueOnChannel:case S::SelectQueueOffChannel:case S::SelectTransmitOnChannel:case S::SelectTransmitOffChannel:case S::SelectLocalPart:channelOwner_=owner;channel_.emplace(StateChannelTask::Kind::SelectedChannel);select(Component::Channel);break;
   case S::LocateLocalControls:partOwner_=PartOwner::Local;part_.emplace(PartLookupTask::Layout::Parameters);select(Component::Part);break;
   case S::QueueNoteStatus:case S::QueueNote:startQueue(t.low(),false);break;
   case S::TransmitNoteStatus:case S::TransmitNote:startSerial(t.low(),false);break;
   default:break;
   }
   else if(t.complete()){
    if(owner==Component::Packet){if(stage==S::FinishQueueNote)startQueue(t.low(),true);else startSerial(t.low(),true);}
    else if(owner==Component::Local){const auto flags=t.flags();const bool accepted=!localEnabled_||!(flags&0x60);const auto event=t.event();local_.reset();route_->resumeChild(event,0,accepted);select(Component::Route);}
    else {const auto event=t.event();route_.reset();controls_.resumeChild(event,false);select(Component::Controls);}
   }break;
  }
  case Component::Channel:{using T=StateChannelTask;
   channel_->advanceBefore(cost,[&](T::Read f){return read(f==T::Read::SelectedPart?0x16798:0x174ae + 20*channel_->part()+11,cost);},[](T::Write,uint8_t){});
   if(channel_->awaitingControls()){partOwner_=PartOwner::Channel;part_.emplace(PartLookupTask::controlsFor(channel_->part()));select(Component::Part);}
   else if(channel_->complete()){const auto event=uint16_t((unsigned(channel_->part())<<8)|channel_->channel());channel_.reset();(channelOwner_==Component::Local?*local_:*packet_).resumeChild(event,0,false);select(channelOwner_);}break;
  }
  case Component::Part:{
   if(part_->stage()==PartLookupTask::Stage::Complete){const auto address=(part_->layout()==PartLookupTask::Layout::Parameters?0x160d2u:0x174aeu)+part_->offset();part_.reset();
    if(partOwner_==PartOwner::Channel){channel_->resumeControls();select(Component::Channel);}else {local_->resumeChild(local_->event(),address,false);select(Component::Local);}
   }else part_->advanceBefore(cost,[&](PartLookupTask::Layout){return read(0x17526,cost);});break;
  }
  case Component::Queue:{using T=InternalMidiQueueTask;
   queue_->advanceBefore(cost,[&](T::Read f){return readWord(f==T::Read::Producer?0x15b04:0x15b06);},[&](T::Write f,uint16_t v){
    if(f==T::Write::Byte||f==T::Write::Overflow)write(f==T::Write::Byte?0x15b08+queue_->index():0x15c08,uint8_t(v),cost);else writeWord(f==T::Write::Producer?0x15b04:0x15b06,v);
   },byteClocks(0x15b08+queue_->index(),previousWrite),3);
   if(queue_->complete()){queue_.reset();finishPacketChild();}break;
  }
  case Component::Serial:
   serial_->advanceBefore(cost,read,write);
   if(serial_->complete()){serial_.reset();finishPacketChild();}break;
  }
  return cost;
 }
private:
 enum class PartOwner{Channel,Local};
 static unsigned wordClocks(uint32_t a){return a>=0xfffd10?2:a>=0x400000?6:3+3*(a&1);}
 static unsigned byteClocks(uint32_t a,int previous){return a<0x400000&&!(previous&1)&&a==(uint32_t(previous)|1u)?0:3;}
 void select(Component component){active_=component;components_|=1u<<unsigned(component);}
 MidiOutputTask*midiTask(){return active_==Component::Route?&*route_:active_==Component::Packet?&*packet_:active_==Component::Local?&*local_:nullptr;}
 const MidiOutputTask*midiTask()const{return active_==Component::Route?&*route_:active_==Component::Packet?&*packet_:active_==Component::Local?&*local_:nullptr;}
 void startQueue(uint8_t value,bool tail){tail_=tail;queue_.emplace(InternalMidiQueueTask::Kind::Push,value);select(Component::Queue);}
 void startSerial(uint8_t value,bool tail){tail_=tail;serial_.emplace(SerialForegroundService::Kind::Transmit,value);select(Component::Serial);}
 void finishPacketChild(){const auto event=packet_->event();if(tail_){packet_.reset();route_->resumeChild(event,0,false);select(Component::Route);}else {packet_->resumeChild(event,0,false);select(Component::Packet);}}
 Kind kind_;Component active_=Component::Controls,channelOwner_=Component::Packet;uint32_t components_=1;bool biasObserved_=false,tail_=false,localEnabled_=false;uint64_t consumed_=0,routed_=0;
 StateForegroundTask controls_{StateForegroundTask::Kind::Controls};PartOwner partOwner_=PartOwner::Channel;
 std::optional<KeyboardEventsTask>keyboard_;std::optional<MidiOutputTask>route_,packet_,local_;std::optional<StateChannelTask>channel_;std::optional<PartLookupTask>part_;std::optional<InternalMidiQueueTask>queue_;std::optional<SerialForegroundService>serial_;
};
}
