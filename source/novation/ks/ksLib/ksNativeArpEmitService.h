#pragma once
#include "ksNativeArpGateService.h"
#include "ksVoiceExpressionBindings.h"
namespace ks {
class NativeArpEmitService final {
public:
 enum class Component{Event,Append,Emission};struct Input{uint8_t part=0;uint16_t noteVelocity=0;};struct Effect{bool read=false,write=false;};
 struct Summary{Input input{};uint8_t part=0,overrideVelocity=0,velocity=0,octave=0;uint16_t retainedNoteVelocity=0,playedNoteVelocity=0,transpose=0;bool appended=false,routed=false;};
 explicit NativeArpEmitService(Input in):event_(ArpEventTask::Kind::Emit,in.part,uint8_t(in.noteVelocity),uint8_t(in.noteVelocity>>8)){summary_.input=in;summary_.part=in.part;summary_.retainedNoteVelocity=in.noteVelocity;summary_.velocity=uint8_t(in.noteVelocity>>8);choose(Component::Event);}
 bool complete()const{return complete_;}bool critical()const{return component_==Component::Append?append_->critical():component_==Component::Emission?emission_->critical():false;}Component component()const{return component_;}uint32_t components()const{return components_;}const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}const ArpEventTask&event()const{return event_;}const NativeArpGateService*append()const{return append_?&*append_:nullptr;}const NativeArpEmissionService*emission()const{return emission_?&*emission_:nullptr;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,NativeArpEmitService>)return this;else if constexpr(std::is_same_v<T,ArpEventTask>){if(component_==Component::Event)return &event_;if(component_==Component::Append)return append_->template task<T>();return emission_->template task<T>();}else {if(component_==Component::Append)return append_->template task<T>();if(component_==Component::Emission)return emission_->template task<T>();return nullptr;}}
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;switch(component_){case Component::Event:return event_.nextClocks(byteClock(0x160d2u+334*event_.part()+322,previous));case Component::Append:return append_->nextClocks(previous);case Component::Emission:return emission_->nextClocks(previous);}return 0;}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};
  if(component_==Component::Append){append_->advanceBefore(cost,read,write,previous);effect_={append_->effect().read,append_->effect().write};if(append_->complete()){summary_.appended=true;summary_.part=append_->summary().part;event_.resumePart(summary_.part);startEmission();}return cost;}
  if(component_==Component::Emission){emission_->advanceBefore(cost,read,write,previous);effect_={emission_->effect().read,emission_->effect().write};if(emission_->complete())complete_=true;return cost;}
  using T=ArpEventTask;using S=T::Stage;const auto stage=event_.stage();const auto base=0x160d2u+334*event_.part();event_.advanceBefore(cost,[&](S q)->uint32_t{switch(q){case S::EmitReadOverride:return summary_.overrideVelocity=uint8_t(read(base+329,cost));case S::EmitReadVelocity:return summary_.velocity=uint8_t(read(base+319,cost));case S::EmitReadOctave:return summary_.octave=uint8_t(read(base+326,cost));default:throw std::logic_error("Unsupported arp emit prefix observation");}},[&](S q,uint32_t v){if(q!=S::EmitPublishLast)throw std::logic_error("Unsupported arp emit prefix publication");write(base+322,uint8_t(v),cost);},[&](S){return byteClock(base+322,previous);});
  if(stage==S::EmitRetainNote){effect_.write=true;summary_.retainedNoteVelocity=uint16_t((unsigned(event_.velocity())<<8)|event_.note());}
  if(stage==S::EmitTranspose){summary_.transpose=uint16_t(summary_.octave*12u);summary_.playedNoteVelocity=uint16_t((unsigned(event_.velocity())<<8)|event_.playedNote());}
  effect_.read=stage==S::EmitReleaseNote||stage==S::EmitFinish;if(event_.awaitingChild()){if(event_.child()!=T::Child::Append)throw std::logic_error("Unsupported arp emit prefix child");effect_.write=true;append_.emplace(NativeArpGateService::Kind::Append,NativeArpGateService::Input{event_.part(),summary_.playedNoteVelocity});choose(Component::Append);}else if(event_.complete())complete_=true;return cost;
 }
 uint32_t expressionSourceAfter(uint32_t prior)const{if(!complete_)throw std::logic_error("Incomplete emit expression binding");prior=(prior&0xffff0000u)|summary_.playedNoteVelocity;if(append_)prior=append_->expressionSourceAfter(prior);if(emission_)return arpEmissionExpressionSourceAfter(*emission_,prior,summary_.retainedNoteVelocity);return(prior&0xffff0000u)|summary_.retainedNoteVelocity;}
 uint32_t notificationAfter(uint32_t prior)const{if(!complete_)throw std::logic_error("Incomplete emit notification binding");if(append_)prior=append_->notificationAfter(prior);return emission_?arpEmissionNotificationAfter(*emission_,prior):prior;}
private:
 static unsigned byteClock(uint32_t a,int previous){return !(previous&1)&&a==(uint32_t(previous)|1u)?0:3;}
 void startEmission(){summary_.routed=true;emission_.emplace(NativeArpEmissionService::Kind::RoutedEmit,NativeArpEmissionService::Input{summary_.part,summary_.playedNoteVelocity});choose(Component::Emission);}
 void choose(Component c){component_=c;components_|=1u<<unsigned(c);}
 ArpEventTask event_;Summary summary_{};Effect effect_{};Component component_=Component::Event;uint32_t components_=0;bool complete_=false;std::optional<NativeArpGateService>append_;std::optional<NativeArpEmissionService>emission_;
};
}
