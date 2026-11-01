#pragma once
#include "ksArpEventTask.h"
#include "ksMidiPacketService.h"
#include "ksNativeNoteProgramService.h"
#include "ksNativeVoiceReleaseService.h"
namespace ks {
// The shared routed note tail. Original logical note retention belongs to the
// typed caller; this graph owns musical routing, publication and voice jobs.
class NativeArpEmissionService final {
public:
 enum class Kind{Off,RoutedEmit};enum class Component{Event,Midi,Release,Program};
 struct Input{uint8_t part=0;uint16_t noteVelocity=0;};struct Effect{bool read=false,write=false;};
 struct Summary{Input input{};uint8_t mask=0,policy=0;bool policyObserved=false,midi=false,released=false,played=false,work=false;};
 NativeArpEmissionService(Kind kind,Input in):kind_(kind),task_(kind==Kind::Off?ArpEventTask::Kind::Off:ArpEventTask::Kind::RoutedEmit,in.part,uint8_t(in.noteVelocity),uint8_t(in.noteVelocity>>8)){summary_.input=in;choose(Component::Event);}
 Kind kind()const{return kind_;}bool complete()const{return complete_;}bool critical()const{return component_==Component::Midi&&midi_->critical();}Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}const ArpEventTask&event()const{return task_;}
 const MidiPacketService*midi()const{return midi_?&*midi_:nullptr;}const NativeVoiceReleaseService*release()const{return release_?&*release_:nullptr;}const NativeNoteProgramService*program()const{return program_?&*program_:nullptr;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,NativeArpEmissionService>)return this;
  else if constexpr(std::is_same_v<T,ArpEventTask>)return component_==Component::Event?&task_:nullptr;
  else if constexpr(std::is_same_v<T,MidiPacketService>)return component_==Component::Midi?midi():nullptr;
  else if constexpr(std::is_same_v<T,NativeVoiceReleaseService>)return component_==Component::Release?release():nullptr;
  else if constexpr(std::is_same_v<T,NativeNoteProgramService>)return component_==Component::Program?program():nullptr;
  else if constexpr(std::is_same_v<T,ControllerDspService>||std::is_same_v<T,DspRoutingTask>||std::is_same_v<T,DspAddressTask>||std::is_same_v<T,DspWordTask>||std::is_same_v<T,DspSelectTask>||std::is_same_v<T,DspFaultTask>||std::is_same_v<T,PartLookupTask>||std::is_same_v<T,VoiceReleaseTask>||std::is_same_v<T,NoteProgramTask>||std::is_same_v<T,VoiceStartTask>||std::is_same_v<T,PeriodicTask>){if(component_==Component::Program)return program_->template task<T>();if(component_==Component::Release)return release_->template task<T>();return nullptr;}
  else if constexpr(std::is_same_v<T,SerialForegroundService>||std::is_same_v<T,MidiOutputTask>||std::is_same_v<T,StateChannelTask>||std::is_same_v<T,InternalMidiQueueTask>)return component_==Component::Midi?midi_->template task<T>():nullptr;
  else return nullptr;
 }
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;switch(component_){case Component::Event:return task_.nextClocks(byteClock(0x160d1,previous));case Component::Midi:return midi_->nextClocks(previous);case Component::Release:return release_->nextClocks(previous);case Component::Program:return program_->nextClocks(previous);}return 0;}
 template<class R,class W>unsigned advanceBefore(unsigned available,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||available<cost)return 0;effect_={};
  if(component_==Component::Midi){midi_->advanceBefore(cost,read,write,previous);effect_={midi_->effect().read,midi_->effect().write};if(midi_->complete()){summary_.midi=true;task_.resumeMidi(summary_.policy);choose(Component::Event);}return cost;}
  if(component_==Component::Release){release_->advanceBefore(cost,read,write,previous);effect_={release_->effect().read,release_->effect().write};if(release_->complete()){summary_.released=true;task_.resumeChild();choose(Component::Event);}return cost;}
  if(component_==Component::Program){program_->advanceBefore(cost,read,write,previous);effect_={program_->effect().read,program_->effect().write};if(program_->complete()){summary_.played=true;task_.resumeChild();choose(Component::Event);}return cost;}
  using T=ArpEventTask;using S=T::Stage;const auto stage=task_.stage();const auto base=0x160d2u+334*task_.part();task_.advanceBefore(cost,[&](S q)->uint32_t{switch(q){case S::OffReadMask:case S::EmitReadMask:return summary_.mask=uint8_t(read(0x160d0,cost));case S::OffReadPolicy:case S::EmitReadPolicy:summary_.policyObserved=true;summary_.policy=uint8_t(read(base+327,cost));return summary_.policy;default:throw std::logic_error("Unsupported emission observation");}},[&](S q,uint32_t v){if(q!=S::EmitPublishWork)throw std::logic_error("Unsupported emission publication");write(0x160d1,uint8_t(v),cost);summary_.work=true;},[&](S){return byteClock(0x160d1,previous);});
  if(stage==S::OffMaskPolicy||stage==S::EmitMaskPolicy)summary_.policy&=0x60;
  if(stage==S::EmitRetainPatch)effect_.write=true;if(stage==S::EmitReleasePatch||stage==S::EmitReleaseNote)effect_.read=true;
  const auto noteVelocity=uint16_t(task_.note()|(unsigned(task_.velocity())<<8));
  if(task_.awaitingChild()){effect_.write=true;switch(task_.child()){
   case T::Child::MidiOff:case T::Child::MidiOn:midi_.emplace(task_.child()==T::Child::MidiOff?MidiPacketService::Kind::NoteOff:MidiPacketService::Kind::NoteOn,MidiPacketService::Input{0,noteVelocity,0});choose(Component::Midi);break;
   case T::Child::VoiceOff:release_.emplace(VoiceReleaseTask::Mode::Note,NativeVoiceReleaseService::Input{task_.note(),0});choose(Component::Release);break;
   case T::Child::Play:program_.emplace(NoteProgramTask::Kind::Select,NativeNoteProgramService::Input{noteVelocity});choose(Component::Program);break;
   default:throw std::logic_error("Unsupported emission child");}
  }else if(task_.complete()){effect_.read=true;complete_=true;}return cost;
 }
 static bool dataSpan(uint32_t a,unsigned n){return NativeVoiceStartService::dataSpan(a,n);}
private:
 static unsigned byteClock(uint32_t raw,int previous){const auto a=raw&0xffffff;return a<0x400000&&!(previous&1)&&a==(uint32_t(previous)|1u)?0:3;}
 void choose(Component c){component_=c;components_|=1u<<unsigned(c);}
 Kind kind_;ArpEventTask task_;Summary summary_{};Effect effect_{};bool complete_=false;Component component_=Component::Event;uint32_t components_=0;
 std::optional<MidiPacketService>midi_;std::optional<NativeVoiceReleaseService>release_;std::optional<NativeNoteProgramService>program_;
};
}
