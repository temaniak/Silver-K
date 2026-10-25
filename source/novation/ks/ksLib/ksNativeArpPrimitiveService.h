#pragma once
#include "ksArpPrimitiveTask.h"
#include "ksPartControlTasks.h"
#include "ksNativeMonoArpBootstrapService.h"
#include <optional>
#include <type_traits>
#include <stdexcept>
namespace ks {
class NativeArpPrimitiveService final {
public:
 using Kind=ArpPrimitiveTask::Kind;enum class Component{Primitive,Lookup,Reset,Mono};struct Input{uint8_t part=0,tempo=0;uint32_t stream=0;};struct Effect{bool read=false,write=false;};
 struct Summary{Input input{};uint8_t part=0;bool changed=false,reset=false,mono=false;};
 NativeArpPrimitiveService(Kind k,Input in):kind_(k),parent_(k,in.part){summary_.input=in;summary_.part=in.part;select(Component::Primitive);}
 Kind kind()const{return kind_;}bool complete()const{return complete_;}bool critical()const{return false;}Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}const ArpPrimitiveTask&primitive()const{return parent_;}const ArpPrimitiveTask*resetTask()const{return reset_?&*reset_:nullptr;}const NativeMonoArpBootstrapService*mono()const{return mono_?&*mono_:nullptr;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,NativeArpPrimitiveService>)return this;else if constexpr(std::is_same_v<T,ArpPrimitiveTask>)return component_==Component::Primitive?&parent_:component_==Component::Reset?&*reset_:nullptr;else if constexpr(std::is_same_v<T,PartLookupTask>)return component_==Component::Lookup?&*lookup_:nullptr;else return component_==Component::Mono?mono_->template task<T>():nullptr;}
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;if(component_==Component::Mono)return mono_->nextClocks(previous);if(component_==Component::Lookup)return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();const auto&t=current();return t.nextClocks(t.publication()?byteClock(target(t,t.publicationKind()),previous):3,t.stage()==ArpPrimitiveTask::Stage::ReadStep?(((t.nextReference()&0xffffff)>=0x400000||(t.nextReference()&1))?6:3):3);}
 template<class R,class W>unsigned advanceBefore(unsigned available,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||available<cost)return 0;effect_={};
  const auto get=[&](uint32_t raw,unsigned n,unsigned back=0){if(!NativeMonoArpBootstrapService::span(raw,n,false))throw std::logic_error("Unsupported arp primitive observation");uint32_t v=0;const auto a=raw&0xffffff;for(unsigned i=0;i<n;++i){const unsigned remain=a>=0x400000?3*(n-i-1):3*((n-i-1+(a+i)%2)/2);v=(v<<8)|uint8_t(read(a+i,cost-back-remain));}return v;};
  const auto put=[&](uint32_t raw,uint32_t v,unsigned n){if(!NativeMonoArpBootstrapService::span(raw,n,true))throw std::logic_error("Unsupported arp primitive publication");for(unsigned i=0;i<n;++i)write((raw+i)&0xffffff,uint8_t(v>>(8*(n-i-1))),cost-3*((n-i-1+(raw+i)%2)/2));};
  if(component_==Component::Mono){mono_->advanceBefore(cost,read,write,previous);effect_={mono_->effect().read,mono_->effect().write};if(mono_->complete()){if(reset_)reset_->resumeChild();else parent_.resumeChild();select(reset_?Component::Reset:Component::Primitive);}return cost;}
  if(component_==Component::Lookup){if(lookup_->stage()==PartLookupTask::Stage::Complete){effect_.read=true;summary_.part=lookup_->index();parent_.resumePart(summary_.part);select(Component::Primitive);}else lookup_->advanceBefore(cost,[&](PartLookupTask::Layout){return read(0x17526,cost);});return cost;}
  using T=ArpPrimitiveTask;using S=T::Stage;auto&t=current();const auto stage=t.stage();const auto base=0x160d2u+334*t.part();t.advanceBefore(cost,[&](T::Read q)->uint32_t{switch(q){case T::Read::Tempo:return get(base+328,1);case T::Read::DesiredTempo:return summary_.input.tempo;case T::Read::Rate:return get(62632+t.rateIndex(),4);case T::Read::Mode:return get(base+324,1);case T::Read::Cursor:return get(base+316,1);case T::Read::Pattern:return get(base+8,4);case T::Read::Step:return get(t.nextReference(),2,2);case T::Read::Length:return get(base+315,1);}throw std::logic_error("Unknown arp primitive observation");},[&](T::Write q,uint32_t v){put(target(t,q),v,q==T::Write::Rate||q==T::Write::Accumulator?4:1);},[&](T::Write q){if(stage==S::ReadStep)return ((t.nextReference()&0xffffff)>=0x400000||(t.nextReference()&1))?6u:3u;return byteClock(target(t,q),previous);});
  effect_.write=stage==S::DispatchPart||stage==S::DispatchReset||stage==S::DispatchMono||stage==S::RetainInput;effect_.read=stage==S::ReleaseInput||stage==S::FinishNull||stage==S::FinishTempo||stage==S::FinishReset||stage==S::FinishPattern;
  if(t.awaitingPart()){lookup_.emplace(PartLookupTask::Layout::Parameters);select(Component::Lookup);}else if(t.awaitingChild()){if(t.stage()==S::AwaitReset){summary_.changed=summary_.reset=true;reset_.emplace(Kind::Reset,t.part());select(Component::Reset);}else {summary_.mono=true;mono_.emplace(NativeMonoArpBootstrapService::Kind::Reset,NativeMonoArpBootstrapService::Input{base,summary_.input.stream});select(Component::Mono);}}
  else if(t.complete()){if(component_==Component::Reset){parent_.resumeChild();select(Component::Primitive);}else complete_=true;}return cost;
 }
private:
 static unsigned byteClock(uint32_t a,int p){a&=0xffffff;return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 static uint32_t target(const ArpPrimitiveTask&t,ArpPrimitiveTask::Write q){using W=ArpPrimitiveTask::Write;unsigned off=0;switch(q){case W::Tempo:off=328;break;case W::Rate:off=4;break;case W::Accumulator:off=60;break;case W::Cursor:case W::CursorZero:off=316;break;case W::PhaseZero:off=304;break;case W::GateZero:off=317;break;}return 0x160d2u+334*t.part()+off;}
 ArpPrimitiveTask&current(){return component_==Component::Reset?*reset_:parent_;}const ArpPrimitiveTask&current()const{return component_==Component::Reset?*reset_:parent_;}void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 Kind kind_;ArpPrimitiveTask parent_;Summary summary_{};Effect effect_{};Component component_=Component::Primitive;uint32_t components_=0;bool complete_=false;std::optional<ArpPrimitiveTask>reset_;std::optional<PartLookupTask>lookup_;std::optional<NativeMonoArpBootstrapService>mono_;
};
}
