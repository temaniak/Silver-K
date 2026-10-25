#pragma once
#include "ksVoiceFeedbackTask.h"
#include "ksPartControlTasks.h"
#include "ksControllerDspService.h"
#include <optional>
#include <type_traits>
namespace ks {
// One physical voice's asynchronous feedback transaction. Semantic observations
// own all continuations; compatibility state is touched only after completion.
class VoiceFeedbackService final {
public:
 using Kind=VoiceFeedbackTask::Kind;
 enum class Component {Feedback,Lookup,Select,Status,Age};
 struct Effect {bool read=false,write=false;};
 struct Summary {uint8_t cursor=0,current=0,count=0,slot=0,status=0;uint16_t observedAge=0,age=0,countdown=0,nextCountdown=0,feedback=0;bool selected=false,activated=false,ageRead=false,countdownRead=false,countdownWritten=false;};
 explicit VoiceFeedbackService(Kind kind=Kind::Poll):kind_(kind),feedback_(kind){}
 Kind kind()const{return kind_;}bool complete()const{return feedback_.stage()==VoiceFeedbackTask::Stage::Complete;}
 Component component()const{return component_;}uint32_t components()const{return components_;}bool critical()const{return false;}
 const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}
 const ControllerDspService*selection()const{return select_?&*select_:nullptr;}
 const ControllerDspService*statusRead()const{return status_?&*status_:nullptr;}
 const ControllerDspService*ageRead()const{return age_?&*age_:nullptr;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,VoiceFeedbackService>)return this;
  else if constexpr(std::is_same_v<T,VoiceFeedbackTask>)return component_==Component::Feedback?&feedback_:nullptr;
  else if constexpr(std::is_same_v<T,PartLookupTask>)return component_==Component::Lookup?&*lookup_:nullptr;
  else if constexpr(std::is_same_v<T,ControllerDspService>)return dsp();
  else {const auto*s=dsp();return s?s->template task<T>():nullptr;}
 }
 unsigned nextClocks(int previous=-1)const{
  if(complete())return 0;
  if(component_==Component::Lookup)return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();
  if(const auto*s=dsp())return s->nextClocks(previous);
  return feedback_.nextClocks([&](VoiceFeedbackTask::Write q){return byteClock(q==VoiceFeedbackTask::Write::Current?0x174a7:0x1755a,previous);});
 }
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  const auto wide=[&](uint32_t at,unsigned n){uint32_t v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|uint8_t(read(at+i,cost-3*((n-1-i)/2)));return v;};
  const auto put=[&](uint32_t at,uint32_t value,unsigned n){for(unsigned i=0;i<n;++i)write(at+i,uint8_t(value>>(8*(n-1-i))),cost-3*((n-1-i)/2));};
  if(component_==Component::Lookup){
   if(lookup_->stage()==PartLookupTask::Stage::Complete){effect_.read=true;summary_.slot=lookup_->index();feedback_.resumeChild(summary_.slot);select(Component::Feedback);}
   else lookup_->advanceBefore(cost,[&](PartLookupTask::Layout){return read(0x174a7,cost);});return cost;
  }
  if(auto*s=dsp()){
   const auto child=component_;s->advanceBefore(cost,read,write,previous);effect_={s->effect().read,s->effect().write};
   if(s->complete()){
    if(child==Component::Select){summary_.selected=true;feedback_.resumeChild();}
    else if(child==Component::Status){summary_.status=uint8_t(s->summary().reply);feedback_.resumeChild(0,summary_.status);}
    else {summary_.feedback=uint16_t(s->summary().reply>>8);summary_.ageRead=true;feedback_.resumeChild(0,0,summary_.feedback);}
    select(Component::Feedback);
   }return cost;
  }
  using T=VoiceFeedbackTask;using S=T::Stage;const auto stage=feedback_.stage();const auto slot=0x17560u+20*feedback_.slot();
  feedback_.advanceBefore(cost,[&](T::Read q)->uint32_t{
   switch(q){case T::Read::Cursor:return summary_.current=uint8_t(read(0x1755a,cost));case T::Read::Count:return summary_.count=uint8_t(read(0x174a6,cost));case T::Read::Age:return summary_.observedAge=uint16_t(wide(slot+8,2));case T::Read::Countdown:summary_.countdownRead=true;return summary_.countdown=uint16_t(wide(slot+10,2));}throw std::logic_error("Unknown feedback observation");
  },[&](T::Write q,uint32_t value){
   switch(q){case T::Write::Current:write(0x174a7,uint8_t(value),cost);break;case T::Write::Cursor:write(0x1755a,uint8_t(value),cost);break;case T::Write::Activated:summary_.activated=true;put(slot+8,value,4);break;case T::Write::Age:put(slot+8,value,2);break;case T::Write::Countdown:summary_.countdownWritten=true;put(slot+10,value,2);break;}
  },[&](T::Write q){return byteClock(q==T::Write::Current?0x174a7:0x1755a,previous);});
  summary_.cursor=feedback_.cursor();summary_.age=feedback_.age();summary_.nextCountdown=feedback_.countdown();
  if(stage==S::Finish)effect_.read=true;
  if(feedback_.child()!=T::Child::None){effect_.write=true;ControllerDspService::Input in;in.port=DspPort::B;
   switch(feedback_.child()){
    case T::Child::Voice:lookup_.emplace(PartLookupTask::Layout::Voice);select(Component::Lookup);break;
    case T::Child::Select:in.routing=DspRoutingTask::Kind::VoiceB;select_.emplace(ControllerDspService::Kind::Routing,in);select(Component::Select);break;
    case T::Child::Status:in.routing=DspRoutingTask::Kind::ReadAlternate;in.address=0x8085;status_.emplace(ControllerDspService::Kind::Routing,in);select(Component::Status);break;
    case T::Child::Age:in.routing=DspRoutingTask::Kind::ReadAlternate;in.address=0x808f;age_.emplace(ControllerDspService::Kind::Routing,in);select(Component::Age);break;
    default:throw std::logic_error("Unknown feedback child");
   }
  }return cost;
 }
private:
 static unsigned byteClock(uint32_t at,int p){return !(p&1)&&at==(uint32_t(p)|1u)?0:3;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 ControllerDspService*dsp(){return component_==Component::Select?&*select_:component_==Component::Status?&*status_:component_==Component::Age?&*age_:nullptr;}
 const ControllerDspService*dsp()const{return component_==Component::Select?&*select_:component_==Component::Status?&*status_:component_==Component::Age?&*age_:nullptr;}
 Kind kind_;VoiceFeedbackTask feedback_;Summary summary_{};Effect effect_{};Component component_=Component::Feedback;uint32_t components_=1;
 std::optional<PartLookupTask>lookup_;std::optional<ControllerDspService>select_,status_,age_;
};
}
