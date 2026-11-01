#pragma once
#include "ksMidiAuxTask.h"
#include "ksNativePedalControlService.h"
#include "ksNativeProgramEventService.h"
#include "ksSysExNativeService.h"
#include "ksPeriodicService.h"
namespace ks {
class NativeMidiAuxService final {
public:
 using Kind=MidiAuxTask::Kind;enum class Component{Aux,Lookup,Parameter,Event,SysEx,Periodic};
 struct Input{uint32_t payload=0,field=0;uint16_t presentation=0;uint8_t controller=0,value=0,patchPart=0;};struct Effect{bool read=false,write=false;};
 struct Summary{Kind kind=Kind::Noop;Input input{};uint8_t part=0;bool parameter=false,event=false,sysEx=false,periodic=false,activationKnown=false;uint8_t activation=0;};
 NativeMidiAuxService(Kind k,Input i):aux_(k){summary_.kind=k;summary_.input=i;if(k==Kind::Parameter)makeParameter(i.controller,i.value);}
 bool complete()const{return complete_;}uint8_t priority()const{if(component_==Component::Event)return event_->priority();if(component_==Component::SysEx)return sysEx_->priority();return critical()?7:0;}bool critical()const{return component_==Component::Parameter&&parameter_->critical()||component_==Component::Event&&event_->critical()||component_==Component::SysEx&&sysEx_->critical()||component_==Component::Periodic&&periodic_->critical();}Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}const MidiAuxTask&auxTask()const{return aux_;}const NativePedalControlService*parameterOutcome()const{return summary_.parameter?&*parameter_:nullptr;}const NativeProgramEventService*eventOutcome()const{return summary_.event?&*event_:nullptr;}const SysExNativeService*sysExOutcome()const{return summary_.sysEx?&*sysEx_:nullptr;}const PeriodicService*periodicOutcome()const{return summary_.periodic?&*periodic_:nullptr;}
 template<class T>const T*task()const{if(complete())return nullptr;if constexpr(std::is_same_v<T,MidiAuxTask>)return component_==Component::Aux?&aux_:nullptr;else if constexpr(std::is_same_v<T,PartLookupTask>){if(component_==Component::Lookup)return &*lookup_;return nested<T>();}else return nested<T>();}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;switch(component_){case Component::Aux:return aux_.nextClocks(aux_.publication()?byteClock(0x160d2u+334*aux_.part()+aux_.publicationOffset(),previous):3);case Component::Lookup:return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();case Component::Parameter:return parameter_->nextClocks(previous);case Component::Event:return event_->nextClocks(previous);case Component::SysEx:return sysEx_->nextClocks(previous);case Component::Periodic:return periodic_->nextClocks(previous);}return 0;}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolve,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
 if(component_==Component::Lookup){if(lookup_->stage()==PartLookupTask::Stage::Complete){summary_.part=lookup_->index();aux_.resumePart(summary_.part);lookup_.reset();effect_.read=true;select(Component::Aux);}else lookup_->advanceBefore(cost,[&](auto){return read(0x17526,cost);});return cost;}
 if(component_==Component::Parameter){parameter_->advanceBefore(cost,read,write,resolve,previous);effect_={parameter_->effect().read,parameter_->effect().write};if(parameter_->complete()){summary_.parameter=true;complete_=true;}return cost;}
 if(component_==Component::Event){event_->advanceBefore(cost,read,write,resolve,previous);effect_={event_->effect().read,event_->effect().write};if(event_->complete()){summary_.event=true;complete_=true;}return cost;}
 if(component_==Component::SysEx){sysEx_->advanceBefore(cost,read,write,resolve,previous);effect_={sysEx_->effect().read,sysEx_->effect().write};if(sysEx_->complete()){summary_.sysEx=true;summary_.activationKnown=sysEx_->activationKnown();if(summary_.activationKnown)summary_.activation=sysEx_->activation();aux_.resumeChild();select(Component::Aux);}return cost;}
 if(component_==Component::Periodic){periodic_->advanceBefore(cost,read,write,previous);effect_={periodic_->effect().read,periodic_->effect().write};if(periodic_->complete()){summary_.periodic=true;complete_=true;}return cost;}
 using T=MidiAuxTask;using S=T::Stage;const auto stage=aux_.stage();const auto base=0x160d2u+334*aux_.part();aux_.advanceBefore(cost,[&](T::Read r)->uint32_t{switch(r){case T::Read::EventFirst:return uint8_t(read(summary_.input.payload&0xffffff,cost));case T::Read::EventSecond:return uint8_t(read((summary_.input.payload+1)&0xffffff,cost));case T::Read::Bank:return uint8_t(read(base+331,cost));default:throw std::logic_error("Unexpected auxiliary MIDI observation");}},[&](T::Write w,uint8_t v){if(w!=T::Write::Bank&&w!=T::Write::InvalidateBank)throw std::logic_error("Unexpected auxiliary MIDI publication");write(base+331,v,cost);},[&](unsigned o){return byteClock(base+o,previous);});
 if(stage==S::GlobalDispatchPart||stage==S::DispatchProgramPart||stage==S::DispatchStart||stage==S::DispatchContinue)effect_.write=true;
 if(stage==S::GlobalFinish||stage==S::FinishStart||stage==S::FinishContinue||stage==S::FinishProgram||stage==S::FinishNoop)effect_.read=true;
 if(aux_.awaitingPart()){lookup_.emplace(PartLookupTask::Layout::Parameters);select(Component::Lookup);}
 else if(aux_.awaitingChild()){if(stage!=S::DispatchStart&&stage!=S::DispatchContinue)throw std::logic_error("Unexpected auxiliary MIDI child");SysExNativeService::Input i;i.payload=summary_.input.payload;i.presentation=summary_.input.presentation;sysEx_.emplace(stage==S::DispatchStart?SysExNativeService::Kind::Begin:SysExNativeService::Kind::End,i);select(Component::SysEx);}
 else if(aux_.stage()==S::CompareLocal)makeParameter(aux_.controller(),aux_.value());
 else if(aux_.handoff()){if(aux_.stage()==S::HandoffProgram){event_.emplace(NativeProgramEventService::Kind::Event,NativeProgramEventService::Input{uint16_t((unsigned(aux_.bank())<<8)|aux_.program()),summary_.input.presentation});select(Component::Event);}else{periodic_.emplace(PeriodicTask::Kind::Manual);select(Component::Periodic);}}
 else if(aux_.complete())complete_=true;return cost;
 }
private:
 template<class T>const T*nested()const{switch(component_){case Component::Parameter:return parameter_->template task<T>();case Component::Event:return event_->template task<T>();case Component::SysEx:return sysEx_->template task<T>();case Component::Periodic:return periodic_->template task<T>();default:return nullptr;}}
 static unsigned byteClock(uint32_t a,int p){return !(p&1)&&a==(unsigned(p)|1u)?0:3;}
 void makeParameter(uint8_t c,uint8_t v){parameter_.emplace(NativePedalControlService::Kind::Parameter,NativePedalControlService::Input{c,v,summary_.input.patchPart,summary_.input.field});select(Component::Parameter);}void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 MidiAuxTask aux_;Component component_=Component::Aux;uint32_t components_=1;bool complete_=false;Effect effect_{};Summary summary_{};std::optional<PartLookupTask>lookup_;std::optional<NativePedalControlService>parameter_;std::optional<NativeProgramEventService>event_;std::optional<SysExNativeService>sysEx_;std::optional<PeriodicService>periodic_;
};
}
