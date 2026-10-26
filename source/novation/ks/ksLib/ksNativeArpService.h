#pragma once
#include "ksArpScheduleTask.h"
#include "ksNativeArpMainService.h"
#include "ksPanelMasksService.h"
namespace ks {
// One complete arp scheduling transaction. The native mono requests are owned
// by NativeArpOwnedService, shared by the central cycle and file transport.
class NativeArpService final {
public:
 enum class Kind{Schedule,Main};enum class Component{Schedule,Main,Masks};enum class Notice{Policy,PartBegin,PartComplete,DrainBegin,DrainComplete};
 using Input=NativeArpMainService::Input;using Effect=NativeArpMainService::Effect;using Noop=NativeArpMainService::Noop;
 struct Summary{Input input{};uint32_t expressionSource=0,parts=0;uint8_t activation=0;};
 NativeArpService(Kind k,Input in):kind_(k){summary_.input=in;summary_.expressionSource=in.expressionSource;summary_.activation=in.activation;if(k==Kind::Main){main_.emplace(in);component_=Component::Main;}}
 Kind kind()const{return kind_;}Component component()const{return component_;}bool complete()const{return complete_;}bool critical()const{return component_==Component::Main&&main_->critical();}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}const ArpScheduleTask&schedule()const{return schedule_;}const NativeArpMainService*main()const{return main_?&*main_:nullptr;}const PanelMasksService*masks()const{return masks_?&*masks_:nullptr;}
 NativeMonoArpCoordinator::Request request()const{return component_==Component::Main?main_->request():NativeMonoArpCoordinator::Request::None;}const NativeMonoArpCoordinator::ChildInput&childInput()const{if(component_!=Component::Main)throw std::logic_error("Missing arp child request");return main_->childInput();}bool resumeInternal(NativeMonoArpCoordinator::ChildResult r){return component_==Component::Main&&main_->resumeInternal(r);}bool resumeFile(NativeMonoArpCoordinator::ChildResult r){return component_==Component::Main&&main_->resumeFile(r);}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,NativeArpService>)return this;else if constexpr(std::is_same_v<T,ArpScheduleTask>)return component_==Component::Schedule?&schedule_:nullptr;else return component_==Component::Main?main_->template task<T>():component_==Component::Masks?masks_->template task<T>():nullptr;}
 unsigned nextClocks(int previous=-1,int previousRead=-1)const{if(complete_)return 0;return component_==Component::Main?main_->nextClocks(previous,previousRead):component_==Component::Masks?masks_->nextClocks(previous):schedule_.nextClocks(schedule_.publication()?byteClock(target(schedule_.publicationKind()),previous):3);}
 template<class R,class W>unsigned advanceBefore(unsigned b,R&&r,W&&w,int previous=-1,int previousRead=-1){return advanceObservedBefore(b,r,w,Noop{},previous,previousRead);}
 template<class R,class W,class O>unsigned advanceObservedBefore(unsigned budget,R&&read,W&&write,O&&observe,int previous=-1,int previousRead=-1){const auto cost=nextClocks(previous,previousRead);if(!cost||cost>budget)return 0;effect_={};const auto notify=[&](Notice n,ArpScheduleTask::Stage st){static_assert(std::is_void_v<decltype(observe(n,st,*this))>);observe(n,st,*this);};using T=ArpScheduleTask;using S=T::Stage;
 if(component_==Component::Main){main_->advanceObservedBefore(cost,read,write,observe,previous,previousRead);effect_=main_->effect();if(main_->complete()){summary_.expressionSource=main_->expressionSourceAfter(summary_.expressionSource);summary_.activation=main_->activationAfter(summary_.activation);++summary_.parts;notify(Notice::PartComplete,schedule_.stage());if(kind_==Kind::Main)complete_=true;else {schedule_.resumeChild();component_=Component::Schedule;}}return cost;}
 if(component_==Component::Masks){masks_->advanceBefore(cost,read,write,previous);effect_={masks_->effect().read,masks_->effect().write};if(masks_->complete()){if(masks_->summary().ran)summary_.activation=uint8_t(masks_->summary().changed?masks_->maskTask().other():masks_->maskTask().mask());notify(Notice::DrainComplete,schedule_.stage());schedule_.resumeChild();component_=Component::Schedule;}return cost;}
 const auto stage=schedule_.stage();schedule_.advanceBefore(cost,[&](T::Read q){return read(q==T::Read::Mask?0x167d3:q==T::Read::Mode||q==T::Read::CurrentMode?0x167ba:q==T::Read::Work?0x160d1:0x160d0,cost);},[&](T::Write q,uint8_t v){write(target(q),v,cost);},[&](T::Write q){return byteClock(target(q),previous);});
 switch(stage){case S::ClearResult:summary_.activation=0;break;case S::ReleaseResult:case S::MarkResult:summary_.activation=schedule_.result();break;default:break;}
 switch(stage){case S::RetainParts:case S::RetainResult:case S::DispatchPart:case S::DispatchDrain:effect_.write=true;break;case S::ReleaseResult:case S::ReleaseParts:case S::Finish:effect_.read=true;break;default:break;}
 notify(Notice::Policy,stage);if(schedule_.stage()==S::AwaitPart){main_.emplace(Input{summary_.expressionSource,schedule_.result()});component_=Component::Main;notify(Notice::PartBegin,stage);}else if(schedule_.stage()==S::AwaitDrain){masks_.emplace(PanelMasksTask::Kind::Mask,PanelMasksService::Input{schedule_.result()});component_=Component::Masks;notify(Notice::DrainBegin,stage);}else if(schedule_.complete())complete_=true;return cost;}
 uint32_t expressionSourceAfter(uint32_t)const{if(!complete_)throw std::logic_error("Incomplete arp source");return summary_.expressionSource;}uint8_t activationAfter(uint8_t)const{if(!complete_)throw std::logic_error("Incomplete arp activation");return summary_.activation;}
private:
 static uint32_t target(ArpScheduleTask::Write w){return w==ArpScheduleTask::Write::Mask?0x160d0:w==ArpScheduleTask::Write::Part?0x17526:0x160d1;}static unsigned byteClock(uint32_t a,int p){return !(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 Kind kind_;ArpScheduleTask schedule_;Summary summary_{};Component component_=Component::Schedule;Effect effect_{};bool complete_=false;std::optional<NativeArpMainService>main_;std::optional<PanelMasksService>masks_;
};
}
