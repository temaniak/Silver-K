#pragma once
#include "ksNoteProgramTask.h"
#include "ksPartControlTasks.h"
#include "ksNativeVoiceStartService.h"
#include "ksPeriodicService.h"
#include <array>
#include <optional>
#include <type_traits>
namespace ks {
class NativeNoteProgramService final {
public:
 using Kind=NoteProgramTask::Kind;enum class Component{Program,Lookup,Start,Periodic};
 struct Input{uint16_t noteVelocity=0;};struct Effect{bool read=false,write=false;};
 struct Summary{Kind kind=Kind::Select;Input input{};uint8_t part=0,controlPart=0;bool played=false,temporary=false;std::optional<NoteProgramTask>selectTask,playTask;std::optional<NativeVoiceStartService>start;std::optional<PeriodicService>periodic;};
 explicit NativeNoteProgramService(Kind k):NativeNoteProgramService(k,Input{}){}NativeNoteProgramService(Kind k,Input i){summary_.kind=k;summary_.input=i;program_[0].emplace(k);}
 bool complete()const{return !depth_;}bool critical()const{return false;}Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,NoteProgramTask>)return component_==Component::Program&&depth_?&*program_[depth_-1]:nullptr;else if constexpr(std::is_same_v<T,PartLookupTask>){if(component_==Component::Lookup)return &*lookup_;return nested<T>();}else if constexpr(std::is_same_v<T,NativeVoiceStartService>)return component_==Component::Start?&*start_:nullptr;else if constexpr(std::is_same_v<T,PeriodicService>)return component_==Component::Periodic?&*periodic_:nullptr;else return nested<T>();}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;switch(component_){case Component::Program:{const auto&t=*program_[depth_-1];return t.nextClocks(t.publication()?byteClock(0x160d2u+334*t.part()+314,previous):3);}case Component::Lookup:return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();case Component::Start:return start_->nextClocks(previous);case Component::Periodic:return periodic_->nextClocks(previous);}return 0;}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  if(component_==Component::Lookup){if(lookup_->stage()==PartLookupTask::Stage::Complete){auto&t=*program_[depth_-1];if(t.awaitingControls()){summary_.controlPart=lookup_->index();t.resumeControls(lookup_->index());}else{summary_.part=lookup_->index();t.resumePart(lookup_->index());}lookup_.reset();effect_.read=true;select(Component::Program);}else lookup_->advanceBefore(cost,[&](auto){return read(0x17526,cost);});return cost;}
  if(component_==Component::Start){start_->advanceBefore(cost,read,write,previous);effect_={start_->effect().read,start_->effect().write};if(start_->complete()){summary_.start=start_;start_.reset();program_[depth_-1]->resumeChild();select(Component::Program);}return cost;}
  if(component_==Component::Periodic){periodic_->advanceBefore(cost,read,write,previous);effect_={periodic_->effect().read,periodic_->effect().write};if(periodic_->complete()){summary_.periodic=periodic_;periodic_.reset();program_[depth_-1]->resumeChild();select(Component::Program);}return cost;}
  using T=NoteProgramTask;using S=T::Stage;auto&t=*program_[depth_-1];const auto stage=t.stage();const auto controls=0x174aeu+20*t.controlPart(),part=0x160d2u+334*t.part();
  const auto wide=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(read(a+n,cost-3*((3-n)/2)));return v;};
  t.advanceBefore(cost,[&](T::Read f)->uint32_t{switch(f){case T::Read::Part:return read(0x17526,cost);case T::Read::Program:{const auto a=92078u+t.tableOffset();return (unsigned(read(a,cost))<<8)|uint8_t(read(a+1,cost));}case T::Read::OriginalReference:return wide(controls);default:return uint8_t(summary_.input.noteVelocity);}},[&](T::Write f,uint32_t v){if(f==T::Write::LastNote)write(part+314,uint8_t(v),cost);else{if(f==T::Write::TemporaryReference)summary_.temporary=true;for(unsigned n=0;n<4;++n)write(controls+n,uint8_t(v>>(24-8*n)),cost-3*((3-n)/2));}},[&](unsigned at){return byteClock(part+at,previous);});
  switch(stage){case S::DispatchControls:case S::RetainOriginal:case S::RetainControls:case S::DispatchPlay:case S::DispatchPart:case S::DispatchUnison:case S::DispatchFeedback:effect_.write=true;break;case S::RestoreControls:case S::RestoreOriginal:case S::FinishTemporary:case S::Finish:effect_.read=true;break;default:break;}
  if(t.awaitingControls()||t.awaitingPart()){lookup_.emplace(t.awaitingControls()?PartLookupTask::Layout::Controls:PartLookupTask::Layout::Parameters);select(Component::Lookup);}else if(t.awaitingChild()){if(t.stage()==S::AwaitPlay){if(depth_==program_.size())throw std::logic_error("Native note program capacity");program_[depth_++].emplace(T::Kind::Play);}else if(t.stage()==S::AwaitUnison){summary_.played=true;start_.emplace(NativeVoiceStartService::Kind::Unison,NativeVoiceStartService::Input{summary_.input.noteVelocity});select(Component::Start);}else{periodic_.emplace(PeriodicTask::Kind::Triggered);select(Component::Periodic);}}
  else if(t.complete()){if(depth_==1&&summary_.kind==Kind::Select)summary_.selectTask=t;else summary_.playTask=t;program_[--depth_].reset();if(depth_)program_[depth_-1]->resumeChild();}
  return cost;
 }
private:
 template<class T>const T*nested()const{if(component_==Component::Start)return start_->template task<T>();if(component_==Component::Periodic)return periodic_->template task<T>();return nullptr;}static unsigned byteClock(uint32_t a,int p){return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 Component component_=Component::Program;uint32_t components_=1;unsigned depth_=1;Effect effect_{};Summary summary_{};std::array<std::optional<NoteProgramTask>,2>program_{};std::optional<PartLookupTask>lookup_;std::optional<NativeVoiceStartService>start_;std::optional<PeriodicService>periodic_;
};
}
