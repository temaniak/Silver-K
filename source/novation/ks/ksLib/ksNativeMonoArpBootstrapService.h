#pragma once
#include <optional>
#include <type_traits>
#include <stdexcept>
#include "ksMonoArpTask.h"
#include "ksMidiFileTimeTask.h"
#include "ksPanelMasksService.h"
namespace ks {
// Native mono-pattern initialization/reset and output-mask transaction. Stream
// and time-record handles remain musical bindings across the VLQ/delay child.
class NativeMonoArpBootstrapService final {
public:
 enum class Kind{Init,Reset,Output};enum class Component{Mono,Delay,Vlq,Output,Masks};
 struct Input{uint32_t record=0x160d2,stream=0;};struct Effect{bool read=false,write=false;};
 struct Summary{Input input{};uint32_t patternStream=0,publishedStream=0,deadline=0,delta=0;uint8_t selected=0,mode=0,enabled=0;bool delayed=false,output=false;};
 NativeMonoArpBootstrapService(Kind k,Input in):kind_(k),parent_(k==Kind::Init?MonoArpTask::Kind::Init:k==Kind::Reset?MonoArpTask::Kind::Reset:MonoArpTask::Kind::Output,in.record,in.stream){summary_.input=in;select(Component::Mono);}
 Kind kind()const{return kind_;}bool complete()const{return complete_;}bool critical()const{return false;}Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}const MonoArpTask&mono()const{return parent_;}const PanelMasksService*masks()const{return masks_?&*masks_:nullptr;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,NativeMonoArpBootstrapService>)return this;else if constexpr(std::is_same_v<T,MonoArpTask>)return component_==Component::Mono?&parent_:component_==Component::Output?&*output_:nullptr;else if constexpr(std::is_same_v<T,MidiFileTimeTask>)return component_==Component::Delay?&*delay_:component_==Component::Vlq?&*vlq_:nullptr;else if constexpr(std::is_same_v<T,PanelMasksService>)return component_==Component::Masks?masks():nullptr;else return component_==Component::Masks?masks_->template task<T>():nullptr;}
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;switch(component_){case Component::Mono:return parent_.nextClocks(monoBus(parent_,previous));case Component::Output:return output_->nextClocks(monoBus(*output_,previous));case Component::Delay:return delay_->nextClocks(delayBus(*delay_,previous));case Component::Vlq:return vlq_->nextClocks(vlqBus(*vlq_));case Component::Masks:return masks_->nextClocks(previous);}return 0;}
 template<class R,class W>unsigned advanceBefore(unsigned available,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||available<cost)return 0;effect_={};
  const auto get=[&](uint32_t raw,unsigned width,unsigned back=0){const auto a=raw&0xffffff;if(!(component_==Component::Vlq?vlqStreamSpan(raw,width):span(raw,width,false)))throw std::logic_error("Unsupported mono bootstrap observation");uint32_t v=0;for(unsigned n=0;n<width;++n){unsigned remain=0;int before=history(a+n);for(unsigned j=n+1;j<width;++j){remain+=byteClock(a+j,before);before=history(a+j);}v=(v<<8)|uint8_t(read(a+n,cost-back-remain));}return v;};
  const auto put=[&](uint32_t raw,uint32_t v,unsigned width){const auto a=raw&0xffffff;if(!span(raw,width,true))throw std::logic_error("Unsupported mono bootstrap publication");for(unsigned n=0;n<width;++n){unsigned remain=0;int before=history(a+n);for(unsigned j=n+1;j<width;++j){remain+=byteClock(a+j,before);before=history(a+j);}write(a+n,uint8_t(v>>(8*(width-n-1))),cost-remain);}};
  if(component_==Component::Masks){masks_->advanceBefore(cost,read,write,previous);effect_={masks_->effect().read,masks_->effect().write};if(masks_->complete()){if(output_){output_->resumeChild();select(Component::Output);}else{parent_.resumeChild();select(Component::Mono);}}return cost;}
  if(component_==Component::Vlq){using T=MidiFileTimeTask;const auto stage=vlq_->stage();vlq_->advanceBefore(cost,[&](T::Stage){return get(vlq_->stream(),1,2);},[](T::Stage,uint32_t){},[&](T::Stage){return vlqBus(*vlq_);});effect_.read=stage==T::Stage::VlqFinish;if(vlq_->complete()){delay_->resumeVlq(vlq_->value(),vlq_->stream());vlq_.reset();select(Component::Delay);}return cost;}
  if(component_==Component::Delay){using T=MidiFileTimeTask;const auto stage=delay_->stage();delay_->advanceBefore(cost,[&](T::Stage){return get(delay_->timeRecord()+56,4);},[&](T::Stage,uint32_t v){summary_.deadline=v;put(delay_->timeRecord()+56,v,4);},[&](T::Stage){return delayBus(*delay_,previous);});effect_.write=stage==T::Stage::ArpReadDelta;effect_.read=stage==T::Stage::ArpFinish;if(delay_->awaitingChild()){vlq_.emplace(T::Kind::Vlq,delay_->stream());select(Component::Vlq);}else if(delay_->complete()){summary_.delta=delay_->value();summary_.publishedStream=delay_->stream();summary_.delayed=true;parent_.resumeDelay(delay_->stream());select(Component::Mono);}return cost;}
  using T=MonoArpTask;using S=T::Stage;auto&t=component_==Component::Output?*output_:parent_;const auto stage=t.stage();t.advanceBefore(cost,[&](S q)->uint32_t{switch(q){case S::InitReadPattern:return get(t.timeRecord()+318,1);case S::InitReadStream:return summary_.patternStream=get(0x11a7a+t.patternIndex(),4);case S::ResetReadPhase:return get(t.timeRecord()+60,4);case S::ResetReadStream:return get(t.timeRecord()+48,4);case S::OutputReadSelected:return summary_.selected=uint8_t(get(0x17526,1));case S::OutputReadMode:return summary_.mode=uint8_t(get(0x167ba,1));case S::OutputReadEnabled:return summary_.enabled=uint8_t(get(0x167d3,1));default:throw std::logic_error("Unsupported mono bootstrap read");}},[&](S q,uint32_t v){const auto at=q==S::InitPublishStream?t.timeRecord()+48:q==S::ResetPublishDeadline?t.timeRecord()+56:t.timeRecord()+52;if(q==S::ResetPublishDeadline)summary_.deadline=v;if(q==S::ResetPublishCursor)summary_.publishedStream=v;put(at,v,4);},[&](S){return monoBus(t,previous);});
  effect_.write=stage==S::ResetRetainStream||stage==S::ResetDelay||stage==S::ResetOutput||stage==S::OutputRetainPart||stage==S::OutputRetainStream||stage==S::OutputPublish;
  effect_.read=stage==S::ResetRestoreStream||stage==S::ResetFinish||stage==S::OutputRestoreStream||stage==S::OutputRestorePart||stage==S::OutputFinish;
  if(t.awaitingChild()){switch(t.child()){case T::Child::Delay:delay_.emplace(MidiFileTimeTask::Kind::ArpDelay,t.stream(),0,t.timeRecord());select(Component::Delay);break;case T::Child::Output:summary_.output=true;output_.emplace(T::Kind::Output,t.timeRecord(),summary_.input.stream);select(Component::Output);break;case T::Child::OutputMask:summary_.output=true;masks_.emplace(PanelMasksTask::Kind::Invert,PanelMasksService::Input{t.mask()});select(Component::Masks);break;default:throw std::logic_error("Unsupported mono bootstrap child");}}
  else if(t.complete()){if(component_==Component::Output){parent_.resumeChild();select(Component::Mono);}else complete_=true;}return cost;
 }
 // VLQ streams can retain addresses in the board's full mapped low SRAM.
 // Keep this separate from time-record and publication admission.
 static bool vlqStreamSpan(uint32_t raw,unsigned n){const auto a=raw&0xffffff;if(span(raw,n,false))return true;if(!n||a>0xffffffu-(n-1))return false;return (a>=0x30000&&a+n<=0x200000)||(a>=0xa30000&&a+n<=0xc00000);}
 static bool span(uint32_t raw,unsigned n,bool writing){const auto a=raw&0xffffff;if(!n||a>0xffffffu-(n-1))return false;if(a>=0x15000&&a+n<=0x30000)return true;if(a>=0xa15000&&a+n<=0xa30000)return true;return !writing&&((a<4&&n<=4-a)||(a>=0xb8e8&&a+n<=0x15000)||(a>=0x800000&&a+n<=0x880000));}
private:
 static int history(uint32_t a){return a<0x400000?int(a):-1;}
 static unsigned byteClock(uint32_t raw,int p){const auto a=raw&0xffffff;return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 static unsigned wideClock(uint32_t a,unsigned n,int p=-1){unsigned v=0;for(unsigned j=0;j<n;++j){v+=byteClock(a+j,p);p=history((a+j)&0xffffff);}return v;}
 static unsigned monoBus(const MonoArpTask&t,int previous){using S=MonoArpTask::Stage;switch(t.stage()){case S::InitReadPattern:return wideClock(t.timeRecord()+318,1);case S::InitReadStream:return wideClock(0x11a7a+t.patternIndex(),4);case S::ResetReadPhase:return wideClock(t.timeRecord()+60,4);case S::ResetReadStream:return wideClock(t.timeRecord()+48,4);case S::InitPublishStream:return wideClock(t.timeRecord()+48,4,previous);case S::ResetPublishDeadline:return wideClock(t.timeRecord()+56,4,previous);case S::ResetPublishCursor:return wideClock(t.timeRecord()+52,4,previous);case S::OutputReadSelected:case S::OutputReadMode:case S::OutputReadEnabled:return 3;default:return 0;}}
 static unsigned delayBus(const MidiFileTimeTask&t,int previous){using S=MidiFileTimeTask::Stage;return t.stage()==S::ArpReadDeadline||t.stage()==S::ArpPublishDeadline?wideClock(t.timeRecord()+56,4,t.stage()==S::ArpPublishDeadline?previous:-1):0;}
 static unsigned vlqBus(const MidiFileTimeTask&t){using S=MidiFileTimeTask::Stage;switch(t.stage()){case S::VlqReadFirst:case S::VlqReadSecond:case S::VlqReadThird:case S::VlqReadFourth:return wideClock(t.stream(),1);default:return 0;}}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 Kind kind_;MonoArpTask parent_;Summary summary_{};Effect effect_{};Component component_=Component::Mono;bool complete_=false;uint32_t components_=0;
 std::optional<MonoArpTask>output_;std::optional<MidiFileTimeTask>delay_,vlq_;std::optional<PanelMasksService>masks_;
};
}
