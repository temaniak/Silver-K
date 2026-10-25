#pragma once
#include "ksInternalNoteTask.h"
#include "ksInternalKeyOutputService.h"
#include "ksNativeArpEmissionService.h"
#include "ksPartControlTasks.h"
#include "ksStateStoreService.h"
namespace ks {
class NativeInternalNoteService final {
public:
 enum class Kind{On,Off};enum class Component{Note,Lookup,Bitmap,Emission};
 struct Input{uint32_t payload=0;};struct Effect{bool read=false,write=false;};
 struct Summary{Kind kind=Kind::On;Input input{};uint8_t part=0;uint16_t noteVelocity=0,retainedNoteVelocity=0;bool retained=false,emitted=false;std::optional<InternalKeyOutputService>bitmap;};
 NativeInternalNoteService(Kind k,Input i):note_(k==Kind::On?InternalNoteTask::Kind::On:InternalNoteTask::Kind::Off,0,0,i.payload){summary_.kind=k;summary_.input=i;}
 bool complete()const{return complete_;}bool critical()const{return component_==Component::Emission&&emission_->critical();}Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}const InternalNoteTask&noteTask()const{return note_;}const NativeArpEmissionService*emissionOutcome()const{return summary_.emitted?&*emission_:nullptr;}
 template<class T>const T*task()const{if(complete())return nullptr;if constexpr(std::is_same_v<T,InternalNoteTask>)return component_==Component::Note?&note_:nullptr;else if constexpr(std::is_same_v<T,PartLookupTask>){if(component_==Component::Lookup)return &*lookup_;return nested<T>();}else return nested<T>();}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;switch(component_){case Component::Note:return note_.nextClocks();case Component::Lookup:return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();case Component::Bitmap:return bitmap_->nextClocks(previous);case Component::Emission:return emission_->nextClocks(previous);}return 0;}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
 if(component_==Component::Lookup){if(lookup_->stage()==PartLookupTask::Stage::Complete){summary_.part=lookup_->index();note_.resumePart(summary_.part);lookup_.reset();effect_.read=true;select(Component::Note);}else lookup_->advanceBefore(cost,[&](auto){return read(0x17526,cost);});return cost;}
 if(component_==Component::Bitmap){bitmap_->advanceBefore(cost,read,write,previous);effect_={bitmap_->effect().read,bitmap_->effect().write};if(bitmap_->complete()){summary_.bitmap=bitmap_;bitmap_.reset();note_.resumeChild();select(Component::Note);}return cost;}
 if(component_==Component::Emission){emission_->advanceBefore(cost,read,write,previous);effect_={emission_->effect().read,emission_->effect().write};if(emission_->complete()){summary_.emitted=true;complete_=true;}return cost;}
 using T=InternalNoteTask;using S=T::Stage;const auto stage=note_.stage();note_.advanceBefore(cost,[&](S q)->uint32_t{if(q!=S::OnReadNote&&q!=S::OnReadVelocity&&q!=S::OffReadNote&&q!=S::OffReadVelocity)throw std::logic_error("Unexpected internal note observation");if(!StateStoreService::dataSpan(note_.payload(),1))throw std::logic_error("Unsupported internal note payload");return uint8_t(read(note_.payload()&0xffffff,cost-(q==S::OnReadNote||q==S::OffReadNote?2:0)));},[](auto,auto){throw std::logic_error("Unexpected internal note publication");},[](auto){return 3u;});summary_.noteVelocity=uint16_t((unsigned(note_.velocity())<<8)|note_.note());
 if(stage==S::OnRetainNote){summary_.retained=true;summary_.retainedNoteVelocity=summary_.noteVelocity;effect_.write=true;}
 if(stage==S::OnBindPart||stage==S::OffBindPart||stage==S::OnSetBitmap||stage==S::OffClearBitmap)effect_.write=true;
 if(note_.awaitingChild()){switch(note_.child()){case T::Child::Part:lookup_.emplace(PartLookupTask::Layout::Parameters);select(Component::Lookup);break;case T::Child::Set:case T::Child::Clear:bitmap_.emplace(note_.child()==T::Child::Set?InternalKeyOutputService::Kind::Set:InternalKeyOutputService::Kind::Clear,InternalKeyOutputService::Input{summary_.part,summary_.noteVelocity});select(Component::Bitmap);break;default:throw std::logic_error("Unexpected internal note child");}}
 else if(note_.handoff()){emission_.emplace(note_.stage()==S::HandoffOn?NativeArpEmissionService::Kind::RoutedEmit:NativeArpEmissionService::Kind::Off,NativeArpEmissionService::Input{summary_.part,summary_.noteVelocity});select(Component::Emission);}return cost;
 }
private:
 template<class T>const T*nested()const{if(component_==Component::Bitmap)return bitmap_->template task<T>();if(component_==Component::Emission)return emission_->template task<T>();return nullptr;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}InternalNoteTask note_;Component component_=Component::Note;bool complete_=false;uint32_t components_=1;Effect effect_{};Summary summary_{};std::optional<PartLookupTask>lookup_;std::optional<InternalKeyOutputService>bitmap_;std::optional<NativeArpEmissionService>emission_;
};
}
