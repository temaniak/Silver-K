#include "ksMidiOutputTask.h"
#include <array>
#include <vector>
#include <iostream>
#include <stdexcept>
using T=ks::MidiOutputTask;using S=T::Stage;using K=T::Kind;
void check(bool value){if(!value)throw std::runtime_error("MIDI output property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned value=0;value<256;++value)for(unsigned channel=0;channel<16;++channel)for(auto kind:{K::NoteOff,K::NoteOn,K::Pedal,K::Expression,K::PitchBend,K::Modulation,K::Pressure,K::QueueOff,K::QueueOn,K::TransmitOff,K::TransmitOn,K::Panic,K::Control,K::RawControl,K::Program}){
  const unsigned key=(value*17)&127,velocity=(value*23)&127;T task(kind,uint16_t(value<<8|key),uint16_t(velocity<<8|key),uint8_t(value));std::vector<unsigned> bytes;unsigned reads=0,writes=0;
  auto read=[&](T::Read field){++reads;if(field==T::Read::SendMode)return 64u;if(field==T::Read::LocalFlags)return 4u;return channel;};auto write=[&](T::Write,unsigned){++writes;};
  auto append=[&]{bytes.push_back(task.low());};
  unsigned steps=0;
  while(!task.complete()){
   const auto stage=task.stage(),before=stage;const auto cost=task.nextClocks();check(cost);const auto count=reads+writes;
   check(!task.advanceBefore(cost-1,read,write)&&task.stage()==before&&reads+writes==count);check(task.advanceBefore(cost,read,write)==cost);++intervals;check(++steps<300);
   switch(stage){
   case S::SendNoteStatus:case S::SendNote:case S::FinishNote:case S::SendPedalStatus:case S::SendPedalController:case S::FinishPedal:case S::SendExpressionStatus:case S::SendExpressionController:case S::FinishExpression:case S::SendBendStatus:case S::SendBendLow:case S::FinishBend:case S::SendModulationStatus:case S::SendModulationController:case S::FinishModulation:case S::SendPressureStatus:case S::FinishPressure:case S::QueueNoteStatus:case S::QueueNote:case S::FinishQueueNote:case S::TransmitNoteStatus:case S::TransmitNote:case S::FinishTransmitNote:case S::SendPanicStatus:case S::SendPanicController:case S::SendPanicValue:case S::SendControlStatus:case S::SendControlIndex:case S::FinishControl:case S::SendProgramStatus:case S::FinishProgram:append();break;default:break;}
   if(task.awaitingChild()){
    unsigned event=task.event();switch(stage){case S::SelectOffChannel:case S::SelectOnChannel:case S::SelectPedalChannel:case S::SelectExpressionChannel:case S::SelectBendChannel:case S::SelectModulationChannel:case S::SelectPressureChannel:case S::SelectQueueOffChannel:case S::SelectQueueOnChannel:case S::SelectTransmitOffChannel:case S::SelectTransmitOnChannel:case S::SelectRawControlChannel:case S::SelectProgramChannel:event=0x300|channel;break;default:break;}
    check(task.resumeChild(uint16_t(event),0x30000,true));
   }
  }
  std::vector<unsigned> expected;
  if(kind==K::NoteOff||kind==K::NoteOn)expected={channel|(kind==K::NoteOn?0x90u:0x80u),key,velocity};
  else if(kind==K::QueueOff||kind==K::TransmitOff||kind==K::QueueOn||kind==K::TransmitOn)expected={channel|((kind==K::QueueOn||kind==K::TransmitOn)?0x90u:0x80u),key,value};
  else if(kind==K::Pedal||kind==K::Expression||kind==K::Modulation)expected={channel|0xb0u,kind==K::Pedal?64u:kind==K::Expression?11u:1u,value};
  else if(kind==K::Pressure)expected={channel|0xd0u,value};
  else if(kind==K::PitchBend)expected={channel|0xe0u,value==255?127u:(value&1)?64u:0u,value>>1};
  else if(kind==K::Panic)expected={channel|0xb0u,123,0};
  else if(kind==K::Control||kind==K::RawControl)expected={channel|0xb0u,key,value};
  else expected={channel|0xc0u,key};
  check(bytes==expected);++cases;
 }
 std::cout<<"PASS: "<<cases<<" MIDI packet policies / "<<intervals<<" intervals; full byte domain, all channels, bend endpoints, packet order and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
