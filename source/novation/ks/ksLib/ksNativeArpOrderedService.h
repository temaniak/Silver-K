#pragma once
#include "ksArpSelectTask.h"
#include "ksNativeArpRandomService.h"
#include "ksNativeArpEmitService.h"
namespace ks {
class NativeArpOrderedService final {
public:
 enum class Component{Select,Random,Emit};struct Input{uint8_t part=0,mode=0,count=0;};struct Effect{bool read=false,write=false;};struct Summary{Input input{};uint8_t index=0,keyOffset=0,nextIndex=0,octave=0,range=0,publishedOctave=0;uint16_t noteVelocity=0;bool wrapped=false,random=false;};
 explicit NativeArpOrderedService(Input in):select_(ArpSelectTask::Kind::Ordered,in.part,in.mode,in.count){summary_.input=in;choose(Component::Select);}
 bool complete()const{return complete_;}bool critical()const{return component_==Component::Emit&&emit_->critical();}Component component()const{return component_;}uint32_t components()const{return components_;}const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}const ArpSelectTask&selection()const{return select_;}const NativeArpRandomService*random()const{return random_?&*random_:nullptr;}const NativeArpEmitService*emit()const{return emit_?&*emit_:nullptr;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,NativeArpOrderedService>)return this;else if constexpr(std::is_same_v<T,ArpSelectTask>){if(component_==Component::Select)return &select_;if(component_==Component::Random)return random_->template task<T>();return emit_->template task<T>();}else {if(component_==Component::Random)return random_->template task<T>();if(component_==Component::Emit)return emit_->template task<T>();return nullptr;}}
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;if(component_==Component::Random)return random_->nextClocks(previous);if(component_==Component::Emit)return emit_->nextClocks(previous);return select_.nextClocks(select_.publication()?byteClock(target(select_.stage()),previous):3);}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};if(component_==Component::Random){random_->advanceBefore(cost,read,write,previous);effect_={random_->effect().read,random_->effect().write};if(random_->complete()){summary_.random=true;select_.resumeRandom(random_->summary().index);choose(Component::Select);}return cost;}if(component_==Component::Emit){emit_->advanceBefore(cost,read,write,previous);effect_={emit_->effect().read,emit_->effect().write};if(emit_->complete()){select_.resumeChild();choose(Component::Select);}return cost;}
  using T=ArpSelectTask;using S=T::Stage;const auto stage=select_.stage();const auto base=0x160d2u+334*select_.part();select_.advanceBefore(cost,[&](S q)->uint32_t{switch(q){case S::OrderedReadIndex:return summary_.index=uint8_t(read(base+308,cost));case S::OrderedReadKey:{summary_.keyOffset=select_.keyOffset();const auto at=base+240+select_.keyOffset();summary_.noteVelocity=uint16_t((unsigned(uint8_t(read(at,cost)))<<8)|uint8_t(read(at+1,cost)));return summary_.noteVelocity;}case S::OrderedReadOctave:summary_.wrapped=true;return summary_.octave=uint8_t(read(base+326,cost));case S::OrderedReadRange:return summary_.range=uint8_t(read(base+325,cost));default:throw std::logic_error("Unsupported arp ordered observation");}},[&](S q,uint32_t v){if(q==S::OrderedPublishIndex)summary_.nextIndex=uint8_t(v);if(q==S::OrderedPublishOctave)summary_.publishedOctave=uint8_t(v);write(target(q),uint8_t(v),cost);},[&](S q){return byteClock(target(q),previous);});
  effect_.write=stage==S::OrderedRandom||stage==S::OrderedRetainPart||stage==S::OrderedEmit;effect_.read=stage==S::OrderedReleasePart||stage==S::OrderedFinish;
  if(select_.awaitingChild()){if(select_.child()==T::Child::Random){random_.emplace(NativeArpRandomService::Input{select_.part()});choose(Component::Random);}else if(select_.child()==T::Child::Emit){emit_.emplace(NativeArpEmitService::Input{select_.part(),select_.key()});choose(Component::Emit);}else throw std::logic_error("Unclosed native arp ordered child");}else if(select_.complete())complete_=true;return cost;
 }
 uint32_t expressionSourceAfter(uint32_t prior)const{if(!complete_||!emit_)throw std::logic_error("Incomplete ordered expression binding");prior=(prior&0xffff0000u)|summary_.noteVelocity;return emit_->expressionSourceAfter(prior);}
 uint32_t notificationAfter(uint32_t prior)const{if(!complete_||!emit_)throw std::logic_error("Incomplete ordered notification binding");if(random_)prior=uint32_t(random_->summary().index)*0x10001u;return emit_->notificationAfter(prior);}
private:
 static unsigned byteClock(uint32_t a,int p){return !(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 uint32_t target(ArpSelectTask::Stage s)const{using S=ArpSelectTask::Stage;const auto base=0x160d2u+334*select_.part();return s==S::OrderedPublishOldOctave?base+321:s==S::OrderedPublishOctave?base+326:base+308;}
 void choose(Component c){component_=c;components_|=1u<<unsigned(c);}
 ArpSelectTask select_;Summary summary_{};Effect effect_{};Component component_=Component::Select;uint32_t components_=0;bool complete_=false;std::optional<NativeArpRandomService>random_;std::optional<NativeArpEmitService>emit_;
};
}
