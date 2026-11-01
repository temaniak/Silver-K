#pragma once
#include "ksKeyActivityTask.h"
#include "ksKeyResetTask.h"
#include "ksKeyPriorityTask.h"
#include "ksPartControlTasks.h"
#include "ksKeyboard.h"
#include <array>
#include <optional>
#include <variant>
#include <stdexcept>
#include <type_traits>
namespace ks {
// Keyboard maps and ordered priority own all lookup/search work. Result words
// and key candidates are musical values; no processor frame is retained.
class NativeKeyService final {
public:
 enum class Kind{Physical,Held,ResetHeld,ResetDown,Compact,Priority,SearchNext,SearchPrevious};
 enum class Component{Activity,Reset,Compact,Priority,Lookup,Search};
 struct Input{uint16_t noteVelocity=0;uint32_t bitmap=0;};
 struct Effect{bool read=false,write=false;};
 struct Summary{Kind kind=Kind::Physical;Input input{};uint8_t part=0,candidate=255,lastCompared=0;uint16_t activity=0,noteVelocity=0;uint32_t bitmap=0;std::optional<KeyActivityTask>activityTask;std::optional<KeyResetTask>resetTask;std::optional<KeyCompactTask>compactTask;std::optional<MonoPriorityTask>priorityTask;std::optional<nativeControl::KeySearchTask>searchTask;};
 explicit NativeKeyService(Kind k):NativeKeyService(k,Input{}){}
 NativeKeyService(Kind k,Input i):kind_(k){summary_.kind=k;summary_.input=i;summary_.noteVelocity=i.noteVelocity;summary_.bitmap=i.bitmap;
  switch(k){case Kind::Physical:push(KeyActivityTask(KeyActivityTask::Kind::Physical));break;case Kind::Held:push(KeyActivityTask(KeyActivityTask::Kind::Held));break;case Kind::ResetHeld:push(KeyResetTask(KeyResetTask::Kind::Held));break;case Kind::ResetDown:push(KeyResetTask(KeyResetTask::Kind::Down));break;case Kind::Compact:push(KeyCompactTask{});break;case Kind::Priority:push(MonoPriorityTask{});break;case Kind::SearchNext:case Kind::SearchPrevious:push(nativeControl::KeySearchTask(k==Kind::SearchNext?nativeControl::KeySearchTask::Direction::Next:nativeControl::KeySearchTask::Direction::Previous,uint8_t(i.noteVelocity)));break;}
 }
 Kind kind()const{return kind_;}Component component()const{return component_;}uint32_t components()const{return components_;}unsigned depth()const{return depth_;}bool complete()const{return !depth_;}bool critical()const{return false;}
 const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 template<class T>const T*task()const{if(!depth_)return nullptr;return std::visit([](const auto&t)->const T*{if constexpr(std::is_same_v<T,std::decay_t<decltype(t)>>)return &t;else return nullptr;},frames_[depth_-1]);}
 unsigned nextClocks(int previous=-1)const{
  if(!depth_)return 0;
  return std::visit([&](const auto&t)->unsigned{using T=std::decay_t<decltype(t)>;
   if constexpr(std::is_same_v<T,KeyResetTask>)return t.complete()?17:t.nextClocks(t.publication()?byteClock(base(t.part())+t.publicationOffset(),previous):3);
   else if constexpr(std::is_same_v<T,KeyCompactTask>)return t.nextClocks(t.publication()?byteClock(base(t.part())+t.publicationOffset(),previous):3);
   else if constexpr(std::is_same_v<T,PartLookupTask>)return t.stage()==T::Stage::Complete?17:t.nextClocks();
   else if constexpr(std::is_same_v<T,nativeControl::KeySearchTask>)return t.complete()?17:t.nextBoundaryClocks();
   else return t.nextClocks();
  },frames_[depth_-1]);
 }
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  const auto word=[&](uint32_t a,unsigned back=0){const auto hi=uint8_t(read(a,cost-back));return uint16_t((unsigned(hi)<<8)|uint8_t(read(a+1,cost-back)));};
  const auto putWord=[&](uint32_t a,uint16_t v,unsigned back=0){write(a,uint8_t(v>>8),cost-back);write(a+1,uint8_t(v),cost-back);};
  std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;
   if constexpr(std::is_same_v<T,PartLookupTask>){if(t.stage()==T::Stage::Complete){const auto part=t.index();summary_.part=part;effect_.read=true;popLookup(part);}else t.advanceBefore(cost,[&](auto){return read(0x17526,cost);});
   }else if constexpr(std::is_same_v<T,KeyActivityTask>){using S=typename T::Stage;const auto stage=t.stage();t.advanceBefore(cost,[&](unsigned at){return word(base(t.part())+at,2);});effect_.write=stage==S::DispatchPart;effect_.read=stage==S::Finish;if(t.awaitingPart())push(PartLookupTask(PartLookupTask::Layout::Parameters));else if(t.complete()){summary_.activity=t.result();summary_.activityTask=t;finish();}
   }else if constexpr(std::is_same_v<T,KeyResetTask>){using S=typename T::Stage;const auto stage=t.stage();if(t.complete()){summary_.resetTask=t;effect_.read=true;finish();return;}t.advanceBefore(cost,[&](unsigned at,uint8_t v){write(base(t.part())+at,v,cost-(stage==S::ClearByte?2:0));},[&](unsigned at){return byteClock(base(t.part())+at,previous);});effect_.write=stage==S::RetainPart||stage==S::RetainBitmap||stage==S::RetainCount||stage==S::DispatchPart;effect_.read=stage==S::ReleaseCount||stage==S::ReleaseBitmap||stage==S::ReleasePart;if(t.awaitingPart())push(PartLookupTask(PartLookupTask::Layout::Parameters));
   }else if constexpr(std::is_same_v<T,KeyCompactTask>){using S=typename T::Stage;const auto stage=t.stage();if(stage==S::CompareKey)summary_.lastCompared=uint8_t(t.key());t.advanceBefore(cost,[&](typename T::Read r,unsigned at)->uint32_t{switch(r){case T::Read::Count:return read(base(t.part())+307,cost);case T::Read::Key:return word(base(t.part())+240+2*at,2);case T::Read::Note:return uint8_t(summary_.input.noteVelocity);}throw std::logic_error("Unsupported compact observation");},[&](typename T::Write w,unsigned at,uint16_t v){if(w==T::Write::Shift)putWord(base(t.part())+240+2*at,v);else write(base(t.part())+at,uint8_t(v),cost);},[&](unsigned at){return byteClock(base(t.part())+at,previous);});effect_.write=stage==S::RetainScratch||stage==S::DispatchPart;effect_.read=stage==S::ReleaseScratch||stage==S::Finish;if(t.awaitingPart())push(PartLookupTask(PartLookupTask::Layout::Parameters));else if(t.complete()){summary_.compactTask=t;finish();}
   }else if constexpr(std::is_same_v<T,MonoPriorityTask>){using S=typename T::Stage;const auto stage=t.stage();t.advanceBefore(cost,[&](typename T::Read r,unsigned at)->uint32_t{switch(r){case T::Read::Candidate:return summary_.candidate;case T::Read::Note:return uint8_t(summary_.input.noteVelocity);case T::Read::Velocity:return read(base(t.part())+64+at,cost);}throw std::logic_error("Unsupported priority observation");});
    effect_.write=stage==S::RetainPart||stage==S::RetainIndex||stage==S::RetainMap||stage==S::RetainDistance||stage==S::RetainCandidate||stage==S::DispatchPart||stage==S::DispatchUpper||stage==S::DispatchLower;effect_.read=stage==S::ReleaseCandidate||stage==S::ReleaseDistance||stage==S::ReleaseMap||stage==S::ReleaseIndex||stage==S::ReleasePart||stage==S::Finish;
    if(t.awaitingPart())push(PartLookupTask(PartLookupTask::Layout::Parameters));else if(t.awaitingUpper()||t.awaitingLower()){summary_.bitmap=base(t.part())+192;push(nativeControl::KeySearchTask(t.awaitingUpper()?nativeControl::KeySearchTask::Direction::Next:nativeControl::KeySearchTask::Direction::Previous,uint8_t(summary_.input.noteVelocity)));}else if(t.complete()){summary_.priorityTask=t;summary_.noteVelocity=uint16_t((unsigned(t.replacement()<128?t.velocity():t.upper())<<8)|t.replacement());finish();}
   }else{if(t.complete()){summary_.candidate=t.terminalKey();summary_.searchTask=t;effect_.read=true;--depth_;if(depth_){auto*p=std::get_if<MonoPriorityTask>(&frames_[depth_-1]);if(!p)throw std::logic_error("Invalid native key search owner");if(p->awaitingUpper())p->resumeUpper();else if(p->awaitingLower())p->resumeLower();else throw std::logic_error("Invalid priority search continuation");select(Component::Priority);}return;}t.advanceBefore(cost,[&](unsigned at){const uint32_t a=summary_.bitmap+at;if(!dataSpan(a))throw std::logic_error("Invalid evolving key bitmap");return read(a&0xffffff,cost);});}
  },frames_[depth_-1]);return cost;
 }
 static bool dataSpan(uint32_t raw){const auto a=raw&0xffffff;return (a>=0x15000&&a<0x30000)||(a>=0xa15000&&a<0xa30000)||(a>=0xffec00&&a<0xffefff);}
private:
 using Task=std::variant<KeyActivityTask,KeyResetTask,KeyCompactTask,MonoPriorityTask,PartLookupTask,nativeControl::KeySearchTask>;
 static uint32_t base(uint8_t n){return 0x160d2u+334*n;}static unsigned byteClock(uint32_t a,int p){return !(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 template<class T>void push(T t){if(depth_==frames_.size())throw std::logic_error("Native key ownership capacity");frames_[depth_++]=std::move(t);if constexpr(std::is_same_v<T,KeyActivityTask>)select(Component::Activity);else if constexpr(std::is_same_v<T,KeyResetTask>)select(Component::Reset);else if constexpr(std::is_same_v<T,KeyCompactTask>)select(Component::Compact);else if constexpr(std::is_same_v<T,MonoPriorityTask>)select(Component::Priority);else if constexpr(std::is_same_v<T,PartLookupTask>)select(Component::Lookup);else select(Component::Search);}
 void finish(){if(depth_!=1)throw std::logic_error("Invalid native key completion owner");--depth_;}
 void popLookup(uint8_t part){if(depth_<2)throw std::logic_error("Invalid native key lookup owner");--depth_;std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,KeyActivityTask>){t.resumePart(part);select(Component::Activity);}else if constexpr(std::is_same_v<T,KeyResetTask>){t.resumePart(part);select(Component::Reset);}else if constexpr(std::is_same_v<T,KeyCompactTask>){t.resumePart(part);select(Component::Compact);}else if constexpr(std::is_same_v<T,MonoPriorityTask>){t.resumePart(part);select(Component::Priority);}else throw std::logic_error("Unsupported native key lookup continuation");},frames_[depth_-1]);}
 Kind kind_;std::array<Task,2>frames_{{KeyCompactTask{},KeyCompactTask{}}};unsigned depth_=0;Component component_=Component::Activity;uint32_t components_=0;Effect effect_{};Summary summary_{};
};
}
