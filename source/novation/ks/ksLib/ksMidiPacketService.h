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
// Complete MIDI publication. Transport choice is a musical request; every
// queued or serial byte retains its own live observations and completion.
class MidiPacketService final {
public:
 enum class Kind {NoteOff,NoteOn,Pedal,Expression,PitchBend,Modulation,Pressure,Byte,QueueOff,QueueOn,TransmitOff,TransmitOn,Panic,Control,RawControl,Program};
 enum class Component {Packet,Channel,Part,Byte,Queue,Serial};
 struct Input {uint16_t event=0,noteVelocity=0;uint8_t control=0;};
 struct Effect {bool read=false,write=false;};
 struct ByteResult {uint16_t event=0,producer=0,consumer=0;uint8_t mode=0;bool queued=false,serial=false,overflow=false;};
 struct Summary {Kind kind=Kind::Byte;Input input{};uint16_t event=0;uint8_t part=0,channel=0,bytes=0;uint32_t controlAddress=0;std::array<ByteResult,3>output{};};
 explicit MidiPacketService(Kind k):MidiPacketService(k,Input{}){}
 MidiPacketService(Kind k,Input i):kind_(k){summary_.kind=k;summary_.input=i;summary_.event=i.event;if(k==Kind::Byte)startByte(i.event,true);else packet_.emplace(packetKind(k),i.event,i.noteVelocity,i.control);}
 Kind kind()const{return kind_;}Component component()const{return component_;}uint32_t components()const{return components_;}
 bool complete()const{return complete_;}bool critical()const{return serial_&&serial_->critical();}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,MidiOutputTask>)return component_==Component::Packet&&packet_?&*packet_:component_==Component::Byte&&byte_?&*byte_:nullptr;
  else if constexpr(std::is_same_v<T,StateChannelTask>)return component_==Component::Channel&&channel_?&*channel_:nullptr;
  else if constexpr(std::is_same_v<T,PartLookupTask>)return component_==Component::Part&&part_?&*part_:nullptr;
  else if constexpr(std::is_same_v<T,InternalMidiQueueTask>)return component_==Component::Queue&&queue_?&*queue_:nullptr;
  else if constexpr(std::is_same_v<T,SerialForegroundService>)return component_==Component::Serial&&serial_?&*serial_:nullptr;
  else return nullptr;
 }
 unsigned nextClocks(int previous=-1)const{
  if(complete_)return 0;
  switch(component_){case Component::Packet:return packet_->nextClocks();case Component::Byte:return byte_->nextClocks();case Component::Channel:return channel_->nextClocks();case Component::Part:return part_->stage()==PartLookupTask::Stage::Complete?17:part_->nextClocks();case Component::Queue:return queue_->nextClocks(byteClocks(0x15b08+queue_->index(),previous),byteClocks(0x15c08,previous));case Component::Serial:return serial_->nextClocks();}return 0;
 }
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){
  const unsigned cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};
  const auto word=[&](uint32_t a){const auto hi=uint8_t(read(a,cost));return uint16_t((unsigned(hi)<<8)|uint8_t(read(a+1,cost)));};
  const auto putWord=[&](uint32_t a,uint16_t v){write(a,uint8_t(v>>8),cost);write(a+1,uint8_t(v),cost);};
  switch(component_){
  case Component::Packet:{using T=MidiOutputTask;using S=T::Stage;const auto stage=packet_->stage();
   packet_->advanceBefore(cost,[&](T::Read f){return read(f==T::Read::ControlChannel?0x169b1:summary_.controlAddress+11,cost);},[](T::Write,uint8_t){});
   effect_.write=packetRetains(stage);effect_.read=packetReleases(stage);
   if(packet_->awaitingChild())switch(stage){
    case S::SelectOffChannel:case S::SelectOnChannel:case S::SelectPedalChannel:case S::SelectExpressionChannel:case S::SelectBendChannel:case S::SelectModulationChannel:case S::SelectPressureChannel:case S::SelectQueueOffChannel:case S::SelectQueueOnChannel:case S::SelectTransmitOffChannel:case S::SelectTransmitOnChannel:case S::SelectRawControlChannel:case S::SelectProgramChannel:
     channel_.emplace(StateChannelTask::Kind::SelectedChannel);select(Component::Channel);break;
    case S::LocatePanicControls:partForChannel_=false;part_.emplace(PartLookupTask::Layout::Controls);select(Component::Part);break;
    case S::QueueNoteStatus:case S::QueueNote:startQueue(packet_->event(),false);break;
    case S::SendNoteStatus:case S::SendNote:case S::TransmitNoteStatus:case S::TransmitNote:case S::SendPanicStatus:case S::SendPanicController:case S::SendPanicValue:case S::SendControlStatus:case S::SendControlIndex:case S::SendProgramStatus:startSerial(packet_->event(),false);break;
    case S::SendPedalStatus:case S::SendPedalController:case S::SendExpressionStatus:case S::SendExpressionController:case S::SendBendStatus:case S::SendBendLow:case S::SendModulationStatus:case S::SendModulationController:case S::SendPressureStatus:startByte(packet_->event(),false);break;
    default:throw std::logic_error("Unsupported MIDI packet child");
   }else if(packet_->complete()){
    switch(stage){case S::FinishPedal:case S::FinishExpression:case S::FinishBend:case S::FinishModulation:case S::FinishPressure:startByte(packet_->event(),true);break;
     case S::FinishQueueNote:startQueue(packet_->event(),true);break;
     case S::ReturnPanic:summary_.event=packet_->event();complete_=true;break;
     default:startSerial(packet_->event(),true);break;}
   }break;
  }
  case Component::Channel:{using T=StateChannelTask;using S=T::Stage;const auto stage=channel_->stage();channel_->advanceBefore(cost,[&](T::Read f){return read(f==T::Read::SelectedPart?0x16798:0x174aeu+20*channel_->part()+11,cost);},[](T::Write,uint8_t){});
   effect_.write=stage==S::AdmitContext||stage==S::LookupControls;effect_.read=stage==S::ReleaseContext||stage==S::ReturnSelected;
   if(channel_->awaitingControls()){partForChannel_=true;part_.emplace(PartLookupTask::controlsFor(channel_->part()));select(Component::Part);}else if(channel_->complete()){summary_.part=channel_->part();summary_.channel=channel_->channel();packet_->resumeChild(uint16_t((unsigned(summary_.part)<<8)|summary_.channel),0,false);channel_.reset();select(Component::Packet);}break;
  }
  case Component::Part:
   if(part_->stage()==PartLookupTask::Stage::Complete){effect_.read=true;summary_.controlAddress=0x174aeu+20*part_->index();part_.reset();if(partForChannel_){channel_->resumeControls();select(Component::Channel);}else{packet_->resumeChild(packet_->event(),summary_.controlAddress,false);select(Component::Packet);}}
   else part_->advanceBefore(cost,[&](PartLookupTask::Layout){return read(0x17526,cost);});break;
  case Component::Byte:{using T=MidiOutputTask;using S=T::Stage;const auto stage=byte_->stage();byte_->advanceBefore(cost,[&](T::Read){return read(0x169ed,cost);},[](T::Write,uint8_t){});
   effect_.write=stage==S::SaveByteContext||stage==S::QueueByte;effect_.read=stage==S::RestoreByteContext;
   if(stage==S::ReadByteMode)current_.mode=byte_->mode();if(byte_->awaitingChild()){current_.queued=true;queue_.emplace(InternalMidiQueueTask::Kind::Push,byte_->low());select(Component::Queue);}else if(byte_->complete()){current_.serial=true;serial_.emplace(SerialForegroundService::Kind::Transmit,byte_->low());select(Component::Serial);}break;
  }
  case Component::Queue:{using T=InternalMidiQueueTask;using S=T::Stage;const auto stage=queue_->stage();queue_->advanceBefore(cost,[&](T::Read f){const auto v=word(f==T::Read::Producer?0x15b04:0x15b06);if(v>255)throw std::logic_error("Invalid live MIDI queue index");return v;},[&](T::Write f,uint16_t v){if(f==T::Write::Byte||f==T::Write::Overflow)write(f==T::Write::Byte?0x15b08+queue_->index():0x15c08,uint8_t(v),cost);else putWord(f==T::Write::Producer?0x15b04:0x15b06,v);},byteClocks(0x15b08+queue_->index(),previous),byteClocks(0x15c08,previous));
   effect_.write=stage==S::SavePush||stage==S::SavePushAddress;effect_.read=stage==S::RestorePush||stage==S::RestorePushAddress||stage==S::ReturnPush;
   if(queue_->complete()){current_.producer=queue_->producer();current_.consumer=queue_->consumer();current_.overflow=queue_->full();queue_.reset();if(byte_){byte_->resumeChild(byte_->event(),0,false);select(Component::Byte);}else finishPublication();}break;
  }
  case Component::Serial:{using S=SerialTransmitTask::Stage;const bool retained=serial_->atTransmit(S::RetainFirst)||serial_->atTransmit(S::RetainSecond)||serial_->atTransmit(S::RetainCritical);const bool released=serial_->atTransmit(S::RestoreCritical)||serial_->atTransmit(S::RestoreSecond)||serial_->atTransmit(S::RestoreFirst)||serial_->atTransmit(S::Finish);
   serial_->advanceBefore(cost,read,write);effect_={released,retained};if(serial_->producer()>255||serial_->consumer()>255)throw std::logic_error("Invalid live MIDI serial index");if(serial_->complete()){current_.producer=serial_->producer();current_.consumer=serial_->consumer();serial_.reset();byte_.reset();finishPublication();}break;
  }
  }return cost;
 }
private:
 static MidiOutputTask::Kind packetKind(Kind k){using T=MidiOutputTask;switch(k){case Kind::NoteOff:return T::Kind::NoteOff;case Kind::NoteOn:return T::Kind::NoteOn;case Kind::Pedal:return T::Kind::Pedal;case Kind::Expression:return T::Kind::Expression;case Kind::PitchBend:return T::Kind::PitchBend;case Kind::Modulation:return T::Kind::Modulation;case Kind::Pressure:return T::Kind::Pressure;case Kind::Byte:return T::Kind::Byte;case Kind::QueueOff:return T::Kind::QueueOff;case Kind::QueueOn:return T::Kind::QueueOn;case Kind::TransmitOff:return T::Kind::TransmitOff;case Kind::TransmitOn:return T::Kind::TransmitOn;case Kind::Panic:return T::Kind::Panic;case Kind::Control:return T::Kind::Control;case Kind::RawControl:return T::Kind::RawControl;case Kind::Program:return T::Kind::Program;}throw std::logic_error("Invalid MIDI packet kind");}
 static unsigned byteClocks(uint32_t a,int p){return !(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 static bool packetRetains(MidiOutputTask::Stage s){using S=MidiOutputTask::Stage;switch(s){
  case S::SavePedal:case S::SaveExpression:case S::SaveBend:case S::SaveBendHigh:case S::SaveModulation:case S::SavePressure:case S::SaveQueueOff:case S::SaveQueueOn:case S::SaveTransmitOff:case S::SaveTransmitOn:case S::SaveControlEvent:case S::SaveControlContext:case S::SaveRawControlEvent:case S::SaveProgramEvent:
  case S::SelectOffChannel:case S::SelectOnChannel:case S::SendNoteStatus:case S::SendNote:case S::SelectPedalChannel:case S::SendPedalStatus:case S::SendPedalController:case S::SelectExpressionChannel:case S::SendExpressionStatus:case S::SendExpressionController:case S::SelectBendChannel:case S::SendBendStatus:case S::SendBendLow:case S::SelectModulationChannel:case S::SendModulationStatus:case S::SendModulationController:case S::SelectPressureChannel:case S::SendPressureStatus:case S::SelectQueueOffChannel:case S::SelectQueueOnChannel:case S::QueueNoteStatus:case S::QueueNote:case S::SelectTransmitOffChannel:case S::SelectTransmitOnChannel:case S::TransmitNoteStatus:case S::TransmitNote:case S::LocatePanicControls:case S::SendPanicStatus:case S::SendPanicController:case S::SendPanicValue:case S::SelectRawControlChannel:case S::SendControlStatus:case S::SendControlIndex:case S::SelectProgramChannel:case S::SendProgramStatus:return true;
  default:return false;}}
 static bool packetReleases(MidiOutputTask::Stage s){using S=MidiOutputTask::Stage;switch(s){case S::RestorePedal:case S::RestoreExpression:case S::RestoreBend:case S::RestoreBendHigh:case S::RestoreModulation:case S::RestorePressure:case S::RestoreQueueNote:case S::RestoreTransmitNote:case S::RestoreControlContext:case S::RestoreControlEvent:case S::RestoreProgramEvent:case S::ReturnPanic:return true;default:return false;}}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void startByte(uint16_t event,bool tail){current_={};current_.event=event;tail_=tail;byte_.emplace(MidiOutputTask::Kind::Byte,event,0,0);select(Component::Byte);}
 void startQueue(uint16_t event,bool tail){current_={};current_.event=event;current_.queued=true;tail_=tail;queue_.emplace(InternalMidiQueueTask::Kind::Push,uint8_t(event));select(Component::Queue);}
 void startSerial(uint16_t event,bool tail){current_={};current_.event=event;current_.serial=true;tail_=tail;serial_.emplace(SerialForegroundService::Kind::Transmit,uint8_t(event));select(Component::Serial);}
 void finishPublication(){if(summary_.bytes==summary_.output.size())throw std::logic_error("MIDI packet outcome capacity");summary_.output[summary_.bytes++]=current_;summary_.event=current_.event;if(tail_)complete_=true;else{packet_->resumeChild(current_.event,0,false);select(Component::Packet);}}
 Kind kind_;Component component_=Component::Packet;uint32_t components_=1;bool complete_=false,tail_=false,partForChannel_=false;Effect effect_{};Summary summary_{};ByteResult current_{};
 std::optional<MidiOutputTask>packet_,byte_;std::optional<StateChannelTask>channel_;std::optional<PartLookupTask>part_;std::optional<InternalMidiQueueTask>queue_;std::optional<SerialForegroundService>serial_;
};
}
