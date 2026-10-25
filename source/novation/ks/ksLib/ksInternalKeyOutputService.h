#pragma once
#include "ksInternalNoteTask.h"
#include "ksPartControlTasks.h"
#include "ksMidiPacketService.h"
#include <array>
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
// Internal pattern-key bitmap mutation and complete release-to-MIDI drain.
// Release does not execute note synthesis: each marked key sends its recovered
// NoteOff packet before the selected live bitmap is cleared.
class InternalKeyOutputService final {
public:
 enum class Kind{Set,Clear,Empty,Release};enum class Component{Notes,Lookup,Packet};
 struct Input{uint8_t part=0;uint16_t noteVelocity=0;};struct Effect{bool read=false,write=false;};
 struct Summary{Kind kind=Kind::Empty;Input input{};uint8_t part=0,notes=0;uint16_t noteVelocity=0,event=0;bool emitted=false,scanned=false;std::optional<InternalNoteTask>task;std::optional<MidiPacketService>packet;};
 explicit InternalKeyOutputService(Kind k):InternalKeyOutputService(k,Input{}){}InternalKeyOutputService(Kind k,Input i):kind_(k){summary_.kind=k;summary_.input=i;summary_.part=i.part;summary_.noteVelocity=i.noteVelocity;notes_[0].emplace(k==Kind::Set?InternalNoteTask::Kind::Set:k==Kind::Clear?InternalNoteTask::Kind::Clear:k==Kind::Empty?InternalNoteTask::Kind::Empty:InternalNoteTask::Kind::Release,i.part,uint8_t(i.noteVelocity));}
 bool complete()const{return !depth_;}bool critical()const{return packet_&&packet_->critical();}Kind kind()const{return kind_;}Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,InternalNoteTask>)return component_==Component::Notes&&depth_?&*notes_[depth_-1]:nullptr;else if constexpr(std::is_same_v<T,PartLookupTask>)return component_==Component::Lookup&&lookup_?&*lookup_:nullptr;else if constexpr(std::is_same_v<T,MidiPacketService>)return component_==Component::Packet&&packet_?&*packet_:nullptr;else return component_==Component::Packet&&packet_?packet_->template task<T>():nullptr;}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;if(component_==Component::Lookup)return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();if(component_==Component::Packet)return packet_->nextClocks(previous);const auto&t=*notes_[depth_-1];return t.nextClocks(t.publication()?byteClock(publication(t),previous):3);}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  if(component_==Component::Lookup){if(lookup_->stage()==PartLookupTask::Stage::Complete){summary_.part=lookup_->index();notes_[depth_-1]->resumePart(lookup_->index());lookup_.reset();effect_.read=true;select(Component::Notes);}else lookup_->advanceBefore(cost,[&](auto){return read(0x17526,cost);});return cost;}
  if(component_==Component::Packet){packet_->advanceBefore(cost,read,write,previous);effect_={packet_->effect().read,packet_->effect().write};if(packet_->complete()){summary_.packet=packet_;summary_.event=packet_->summary().event;summary_.emitted=true;++summary_.notes;packet_.reset();notes_[depth_-1]->resumeChild();select(Component::Notes);}return cost;}
  using T=InternalNoteTask;using S=T::Stage;auto&t=*notes_[depth_-1];const auto stage=t.stage();const auto base=0x160d2u+334*t.part();if(stage==S::ReleaseSetVelocity)summary_.scanned=true;
  t.advanceBefore(cost,[&](S s)->uint32_t{switch(s){case S::SetModify:case S::ClearModify:return read(base+224+t.group(),cost-byteClock(publication(t),previous));case S::ReleaseReadPolicy:return read(base+327,cost);case S::ReleaseReadByte:return read(base+224+t.cursor(),cost-2);default:throw std::logic_error("Unsupported internal key observation");}},[&](S s,uint8_t v){write(s==S::EmptyPublish?base+224+t.cursor():base+224+t.group(),v,cost-(s==S::EmptyPublish?2:0));},[&](S){return byteClock(publication(t),previous);});
  effect_.write=stage==S::EmptyBindPart||stage==S::ReleaseBindPart||stage==S::ReleaseMidi||stage==S::ReleaseClearAll;effect_.read=stage==S::SetFinish||stage==S::ClearFinish||stage==S::EmptyFinish||stage==S::ReleaseFinish;
  if(t.awaitingChild())switch(t.child()){case T::Child::Part:lookup_.emplace(PartLookupTask::Layout::Parameters);select(Component::Lookup);break;case T::Child::Midi:packet_.emplace(MidiPacketService::Kind::NoteOff,MidiPacketService::Input{0,uint16_t((unsigned(t.velocity())<<8)|t.note()),0});select(Component::Packet);break;case T::Child::Empty:if(depth_==notes_.size())throw std::logic_error("Internal key owner capacity");notes_[depth_++].emplace(T::Kind::Empty);break;default:throw std::logic_error("Unsupported internal key child");}
  else if(t.complete()){if(kind_==Kind::Set||kind_==Kind::Clear)summary_.part=t.part();if(depth_==1){summary_.task=t;if(summary_.scanned)summary_.noteVelocity=uint16_t((unsigned(t.velocity())<<8)|t.note());notes_[--depth_].reset();}else{notes_[--depth_].reset();notes_[depth_-1]->resumeChild();}}
  return cost;
 }
private:
 static unsigned byteClock(uint32_t a,int p){return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 static uint32_t publication(const InternalNoteTask&t){using S=InternalNoteTask::Stage;return 0x160d2u+334*t.part()+224+(t.stage()==S::EmptyPublish?t.cursor()-1:t.group());}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 Kind kind_;Component component_=Component::Notes;uint32_t components_=1;unsigned depth_=1;Effect effect_{};Summary summary_{};std::array<std::optional<InternalNoteTask>,2>notes_{};std::optional<PartLookupTask>lookup_;std::optional<MidiPacketService>packet_;
};
}
