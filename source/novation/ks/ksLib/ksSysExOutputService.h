#pragma once
#include "ksSysExActionTask.h"
#include "ksSerialDirectTask.h"
#include "ksSerialForegroundService.h"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace ks {
// Complete output-only SysEx header/send graphs. Byte output follows the
// original direct UART path, including live ready polling. It does not enqueue
// bytes into the unrelated interrupt-driven foreground transmit queue.
class SysExOutputService final {
public:
 enum class Kind {Header,Send,SendHeader,Byte,ResetRx};
 enum class Component {Action,ByteJump,Direct,Reset};
 struct Input {uint32_t source=0;uint16_t count=0;uint8_t byte=0;};
 struct Effect {bool read=false,write=false;};
 struct Summary {uint32_t sourceEnd=0;uint16_t countRemaining=0,headerIndex=0;uint8_t lastByte=0,headerLast=0,status=0;uint64_t bytesSent=0;};
 explicit SysExOutputService(Kind kind):SysExOutputService(kind,Input{}){}
 SysExOutputService(Kind kind,Input input):kind_(kind),input_(input){
  if(!admitted(kind,input))throw std::logic_error("Unsupported SysEx output data span");
  if(kind==Kind::ResetRx){reset_.emplace(SerialForegroundService::Kind::ResetRx);select(Component::Reset);}
  else if(kind==Kind::Byte){summary_.lastByte=input.byte;select(Component::ByteJump);}
  else {action_.emplace(kind==Kind::Header?SysExActionTask::Kind::Header:kind==Kind::Send?SysExActionTask::Kind::Send:SysExActionTask::Kind::SendHeader,0x16040,input.source,input.count);select(Component::Action);}
 }
 static bool readable(uint32_t raw,unsigned size){
  const auto a=raw&0xffffffu;
  return size&&((a<0x200000&&size<=0x200000-a)||(a>=0x81ff00&&a<0x860000&&size<=0x860000-a)||(a>=0xa00000&&a<0xc00000&&size<=0xc00000-a)||(a>=0xffdc00&&a<0xfff000&&size<=0xfff000-a));
 }
 static bool admitted(Kind kind,Input input){return kind!=Kind::Send||readable(input.source,input.count?unsigned(input.count):65536u);}
 Kind kind()const{return kind_;}Component component()const{return component_;}uint32_t components()const{return components_;}
 bool complete()const{return complete_;}bool critical()const{return component_==Component::Reset&&reset_&&reset_->critical();}
 const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 const SysExActionTask*actionTask()const{return nested_?&*nested_:action_?&*action_:nullptr;}
 const SerialDirectTask*directTask()const{return direct_?&*direct_:nullptr;}
 const SerialForegroundService*resetTask()const{return reset_?&*reset_:nullptr;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,SysExActionTask>)return component_==Component::Action?actionTask():nullptr;
  else if constexpr(std::is_same_v<T,SerialDirectTask>)return component_==Component::Direct?directTask():nullptr;
  else return nullptr;
 }
 unsigned nextClocks(int previous=-1)const{
  if(complete_)return 0;
  if(component_==Component::ByteJump)return 8;
  if(component_==Component::Reset)return reset_->nextClocks();
  if(component_==Component::Direct)return direct_->nextClocks([&](SerialDirectTask::Write w){return byteClock(w==SerialDirectTask::Write::Data?0xffff8b:0xffff8c,previous);});
  const auto*t=actionTask();return t->nextClocks(t->publication()?byteClock(0x16044+t->index(),previous):3);
 }
 template<class Reader,class Writer,class Resolver>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,Resolver&&,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>available)return 0;effect_={};
  if(component_==Component::ByteJump){direct_.emplace(SerialDirectTask::Kind::Write);select(Component::Direct);return cost;}
  if(component_==Component::Reset){
   return advanceReset(cost,read,write);
  }
  if(component_==Component::Direct){
   using T=SerialDirectTask;const auto stage=direct_->stage();
   direct_->advanceBefore(cost,[&](T::Read field){return field==T::Read::Input?summary_.lastByte:uint8_t(read(0xffff8c,cost));},
    [&](T::Write field,uint8_t value){write(field==T::Write::Data?0xffff8b:0xffff8c,value,cost);},
    [&](T::Write field){return byteClock(field==T::Write::Data?0xffff8b:0xffff8c,previous);});
   effect_.write=stage==T::Stage::Hold;effect_.read=stage==T::Stage::Restore||stage==T::Stage::Finish;
   summary_.status=direct_->status();
   if(direct_->stage()==T::Stage::Complete){
    ++summary_.bytesSent;direct_.reset();
    if(kind_==Kind::Byte)complete_=true;
    else {auto*t=mutableAction();if(!t->resumeChild())throw std::logic_error("Missing SysEx output byte continuation");select(Component::Action);}
   }
   return cost;
  }
  using T=SysExActionTask;using S=T::Stage;auto*t=mutableAction();const auto stage=t->stage();
  const auto readData=[&](S s)->uint32_t{
   if(s==S::HeaderRead){summary_.headerLast=uint8_t(read(0x1b0+t->index(),cost-2));return summary_.headerLast;}
   if(s==S::SendRead){const auto address=t->source();if(!readable(address,1))throw std::logic_error("Unsupported evolving SysEx source");return uint8_t(read(address&0xffffff,cost-2));}
   throw std::logic_error("Unexpected SysEx output read");
  };
  const auto writeData=[&](S s,uint32_t value){if(s!=S::HeaderPublish)throw std::logic_error("Unexpected SysEx output publication");write(0x16044+t->index(),uint8_t(value),cost);};
  t->advanceBefore(cost,readData,writeData,[&](S){return byteClock(0x16044+t->index(),previous);});
  effect_.write=stage==S::SendByte||stage==S::SendHeaderByte||stage==S::SendHeaderData;
  effect_.read=stage==S::HeaderFinish||stage==S::SendFinish||stage==S::SendHeaderFinish;
  if(stage==S::SendRead||stage==S::SendMask||stage==S::SendHeaderStatus)summary_.lastByte=t->byte();
  if(stage==S::HeaderAdvance)summary_.headerIndex=t->index();
  if(stage==S::SendRead||stage==S::SendTick||stage==S::SendHeaderBind||stage==S::SendHeaderCount){summary_.sourceEnd=t->source();summary_.countRemaining=t->remaining();}
  if(t->awaitingChild()){
   if(t->child()==T::Child::Serial){summary_.lastByte=t->byte();select(Component::ByteJump);}
   else if(t->child()==T::Child::Send){
    if(nested_)throw std::logic_error("SysEx output nesting capacity");nested_.emplace(T::Kind::Send,0x16040,t->source(),t->remaining());select(Component::Action);
   }else throw std::logic_error("Unexpected SysEx output child");
  }else if(t->complete()){
   if(nested_){summary_.sourceEnd=nested_->source();summary_.countRemaining=nested_->remaining();nested_.reset();if(!action_->resumeChild())throw std::logic_error("Missing header send continuation");}
   else {if(kind_==Kind::Header){summary_.sourceEnd=0x1b0+summary_.headerIndex;summary_.countRemaining=0;}complete_=true;}
  }
  return cost;
 }
private:
 template<class Reader,class Writer>unsigned advanceReset(unsigned cost,Reader&&read,Writer&&write){
  using T=SerialQueueTask;effect_.write=reset_->atQueue(T::Stage::RetainCritical)||reset_->atQueue(T::Stage::RetainWord);
  effect_.read=reset_->atQueue(T::Stage::RestoreWord)||reset_->atQueue(T::Stage::RestoreCritical)||reset_->atQueue(T::Stage::Finish);
  reset_->advanceBefore(cost,read,write);if(reset_->complete())complete_=true;return cost;
 }
 static unsigned byteClock(uint32_t a,int previous){
  a&=0xffffff;if(a<0x400000)return(previous&1)||a!=(unsigned(previous)|1u)?3:0;
  if(a>=0xfffd10&&a<0xffffa0)return(previous&1)||a!=(unsigned(previous)|1u)?1:0;
  return a>=0xffffa0?1:3;
 }
 SysExActionTask*mutableAction(){return nested_?&*nested_:&*action_;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 Kind kind_;Input input_;Component component_=Component::Action;bool complete_=false;uint32_t components_=0;Effect effect_{};Summary summary_{};
 std::optional<SysExActionTask>action_,nested_;std::optional<SerialDirectTask>direct_;std::optional<SerialForegroundService>reset_;
};

// Only used once a complete semantic child has returned to a still-legacy
// caller. No ABI/register shadow is retained while SysExOutputService executes.
template<class A>void projectSysExOutput(A&a,const SysExOutputService&s){
 if(!s.complete())throw std::logic_error("Incomplete SysEx output projection");
 using K=SysExOutputService::Kind;const auto&f=s.summary();auto&v=a.values;
 const auto nz16=[](uint16_t w){return uint8_t((w&0x8000?8:0)|(!w?4:0));};
 if(s.kind()==K::Header){v[0]=0x1bc;v[1]=0x16050;v[2]=(v[2]&0xff00u)|f.headerLast;a.flags=uint8_t((a.flags&0xf1)|4);}
 else if(s.kind()==K::Send||s.kind()==K::SendHeader){v[0]=(v[0]&0xffff00ffu)|(uint32_t(f.lastByte)<<8);v[5]&=0xffff0000u;v[6]=f.sourceEnd;a.flags=uint8_t((a.flags&0xf1)|4);}
 else if(s.kind()==K::Byte)a.flags=uint8_t((a.flags&0xf1)|nz16(uint16_t(v[0])));
 else a.flags=uint8_t((a.flags&0xd0)|nz16(uint16_t(v[0])));
}
}
