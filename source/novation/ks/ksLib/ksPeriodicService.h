#pragma once
#include "ksPeriodicTask.h"
#include "ksSyncValueTask.h"
#include "ksControllerDspService.h"
#include <array>
#include <optional>
#include <type_traits>
namespace ks {
// Native periodic synchronization owns all five conversion and DSP children.
// Only musical bytes and observed protocol outcomes survive a suspension.
class PeriodicService final {
public:
 using Kind=PeriodicTask::Kind;
 enum class Component{Periodic,Value,Dsp};
 struct Effect{bool read=false,write=false;};
 struct ValueOutcome {SyncValueTask value;std::optional<ControllerDspService>dsp;explicit ValueOutcome(SyncValueTask::Kind k):value(k){}};
 struct Summary{uint8_t mode=0,trigger=0,children=0;std::array<uint8_t,5>input{},secondary{};};
 explicit PeriodicService(Kind kind=Kind::Fast):kind_(kind),parent_(kind){}
 Kind kind()const{return kind_;}bool complete()const{return parent_.stage()==PeriodicTask::Stage::Complete;}bool critical()const{return false;}
 Component component()const{return component_;}uint32_t components()const{return components_;}
 const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 const ValueOutcome*value(unsigned n)const{return n<5&&values_[n]?&*values_[n]:nullptr;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,PeriodicService>)return this;
  else if constexpr(std::is_same_v<T,PeriodicTask>)return component_==Component::Periodic?&parent_:nullptr;
  else if constexpr(std::is_same_v<T,SyncValueTask>)return component_==Component::Value?&values_[index_]->value:nullptr;
  else if constexpr(std::is_same_v<T,ControllerDspService>)return component_==Component::Dsp?&*values_[index_]->dsp:nullptr;
  else return component_==Component::Dsp?values_[index_]->dsp->template task<T>():nullptr;
 }
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;if(component_==Component::Dsp)return values_[index_]->dsp->nextClocks(previous);if(component_==Component::Value)return values_[index_]->value.nextClocks();return parent_.nextClocks();}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  if(component_==Component::Dsp){auto&s=*values_[index_]->dsp;s.advanceBefore(cost,read,write,previous);effect_={s.effect().read,s.effect().write};if(s.complete()){values_[index_]->value.resumeChild();select(Component::Value);}return cost;}
  if(component_==Component::Value){using T=SyncValueTask;const auto stage=values_[index_]->value.stage();auto&t=values_[index_]->value;
   t.advanceBefore(cost,[&](T::Read f)->uint32_t{if(f==T::Read::Input)return summary_.input[index_];if(f==T::Read::Secondary)return summary_.secondary[index_];const auto a=0x1532eu+t.value();uint32_t result=0;for(unsigned n=0;n<4;++n)result=(result<<8)|uint8_t(read(a+n,cost-3*((3-n)/2)));return result;});
   effect_.read=stage==T::Stage::FinishDivision||stage==T::Stage::FinishShape;
   if(t.awaiting()){effect_.write=true;ControllerDspService::Input in;in.routing=index_<3?DspRoutingTask::Kind::WriteA:DspRoutingTask::Kind::WriteB;in.port=index_<3?DspPort::A:DspPort::B;constexpr uint16_t addresses[]{0x824d,0x8250,0x824c,0x8086,0x8089};in.address=addresses[index_];in.word=t.wire();values_[index_]->dsp.emplace(ControllerDspService::Kind::Routing,in);select(Component::Dsp);}
   else if(t.stage()==T::Stage::Complete){++summary_.children;parent_.resumeChild();select(Component::Periodic);}return cost;
  }
  using T=PeriodicTask;using S=T::Stage;const auto stage=parent_.stage();
  parent_.advanceBefore(cost,[&](unsigned field){const auto v=uint8_t(read(field==T::TriggerField?0x1755c:0x168fc+field,cost));if(field==T::TriggerField)summary_.trigger=v;else if(field==249)summary_.mode=v;return v;},[&](uint8_t value){write(0x1755c,value,cost);});
  switch(stage){case S::MarkRetain:case S::HoldFast:case S::HoldTrigger:case S::HoldManual:case S::RetainPatch:case S::RetainUpper:case S::RetainAddress:case S::RetainValue:case S::RetainPort:effect_.write=true;break;
   case S::MarkRestore:case S::MarkFinish:case S::SkipRestore:case S::SkipFinish:case S::RestorePort:case S::RestoreValue:case S::RestoreAddress:case S::RestoreUpper:case S::RestorePatch:case S::RestoreInput:case S::Finish:effect_.read=true;break;default:break;}
  if(parent_.child()!=T::Child::None){effect_.write=true;index_=unsigned(parent_.child())-1;summary_.input[index_]=parent_.input();summary_.secondary[index_]=parent_.secondary();values_[index_].emplace(index_<3?SyncValueTask::Kind::Division:SyncValueTask::Kind::Shape);select(Component::Value);}
  return cost;
 }
private:
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 Kind kind_;PeriodicTask parent_;Component component_=Component::Periodic;uint32_t components_=1;unsigned index_=0;Effect effect_{};Summary summary_{};std::array<std::optional<ValueOutcome>,5>values_;
};
}
