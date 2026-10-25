#pragma once
#include "ksPartRefreshTask.h"
#include "ksGlobalUpdateService.h"
#include <array>
#include <optional>
#include <type_traits>
namespace ks {
// Four physical DSP feedback mailboxes. Clear and global refresh remain owned
// by the same native transaction; the selected controller part is restored.
class PartFeedbackService final {
public:
 enum class Component{Feedback,Status,Clear,Global};
 struct Effect{bool read=false,write=false;};
 struct Summary{uint8_t current=0;std::array<uint8_t,4>status{},base{},part{};uint8_t refreshed=0;};
 bool complete()const{return parent_.stage()==PartRefreshTask::Stage::Complete;}bool critical()const{return false;}
 Component component()const{return component_;}uint32_t components()const{return components_;}
 const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}
 const ControllerDspService*status(unsigned n)const{return n<4&&status_[n]?&*status_[n]:nullptr;}
 const ControllerDspService*clear(unsigned n)const{return n<4&&clear_[n]?&*clear_[n]:nullptr;}
 const GlobalUpdateService*global(unsigned n)const{return n<4&&global_[n]?&*global_[n]:nullptr;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,PartFeedbackService>)return this;
  else if constexpr(std::is_same_v<T,PartRefreshTask>)return component_==Component::Feedback?&parent_:nullptr;
  else if constexpr(std::is_same_v<T,GlobalUpdateService>)return component_==Component::Global?global(parent_.index()):nullptr;
  else if constexpr(std::is_same_v<T,ControllerDspService>){if(component_==Component::Status)return status(parent_.index());if(component_==Component::Clear)return clear(parent_.index());if(component_==Component::Global)return global(parent_.index())->dsp();return nullptr;}
  else {if(component_==Component::Status)return status(parent_.index())->template task<T>();if(component_==Component::Clear)return clear(parent_.index())->template task<T>();if(component_==Component::Global)return global(parent_.index())->template task<T>();return nullptr;}
 }
 unsigned nextClocks(int previous=-1)const{
  if(complete())return 0;const auto n=parent_.index();switch(component_){case Component::Feedback:return parent_.nextClocks();case Component::Status:return status_[n]->nextClocks(previous);case Component::Clear:return clear_[n]->nextClocks(previous);case Component::Global:return global_[n]->nextClocks(previous);}return 0;
 }
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};const auto n=parent_.index();
  if(component_==Component::Global){auto&s=*global_[n];s.advanceBefore(cost,read,write,previous);effect_={s.effect().read,s.effect().write};if(s.complete()){summary_.refreshed|=uint8_t(1u<<n);parent_.resumeChild();select(Component::Feedback);}return cost;}
  if(component_==Component::Status||component_==Component::Clear){const bool status=component_==Component::Status;auto&s=status?*status_[n]:*clear_[n];s.advanceBefore(cost,read,write,previous);effect_={s.effect().read,s.effect().write};if(s.complete()){if(status){summary_.status[n]=uint8_t(s.summary().reply);parent_.resumeChild(summary_.status[n]);}else parent_.resumeChild();select(Component::Feedback);}return cost;}
  using T=PartRefreshTask;using S=T::Stage;const auto stage=parent_.stage();
  parent_.advanceBefore(cost,[&](T::Read f){const auto v=uint8_t(read(f==T::Read::Current?0x17526:0x167ba,cost));if(f==T::Read::Current)summary_.current=v;else summary_.base[n]=v;return v;},[&](uint8_t value){write(0x17526,value,cost);});
  switch(stage){case S::RetainCurrent:case S::HoldIndex:case S::HoldAddress:case S::HoldForClear:case S::HoldIndexForRefresh:case S::HoldAddressForRefresh:effect_.write=true;break;
   case S::RestoreForClear:case S::RestoreForRefresh:case S::RestoreIndexForRefresh:case S::RestoreAddress:case S::RestoreIndex:case S::RestoreCurrent:case S::Finish:effect_.read=true;break;default:break;}
  if(stage==S::ResolvePart)summary_.part[n]=parent_.part();
  if(parent_.child()!=T::Child::None){effect_.write=true;ControllerDspService::Input in;in.port=DspPort::A;in.address=parent_.address();
   switch(parent_.child()){case T::Child::Status:status_[n].emplace(ControllerDspService::Kind::Read,in);select(Component::Status);break;
    case T::Child::Clear:in.routing=DspRoutingTask::Kind::WriteA;clear_[n].emplace(ControllerDspService::Kind::Routing,in);select(Component::Clear);break;
    case T::Child::Global:global_[n].emplace(GlobalUpdateService::Input{0,DspPort::A,parent_.address()});select(Component::Global);break;
    default:throw std::logic_error("Unknown part feedback child");}
  }return cost;
 }
private:
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 PartRefreshTask parent_;Component component_=Component::Feedback;uint32_t components_=1;Summary summary_{};Effect effect_{};
 std::array<std::optional<ControllerDspService>,4>status_,clear_;std::array<std::optional<GlobalUpdateService>,4>global_;
};
}
