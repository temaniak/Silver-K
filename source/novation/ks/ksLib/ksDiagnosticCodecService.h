#pragma once
#include "ksDiagnosticCodecTask.h"
#include "ksSerialDirectTask.h"
#include <array>
#include <optional>
#include <type_traits>
#include <stdexcept>
namespace ks {
class DiagnosticUartService final {
public:
 struct Effect{bool read=false,write=false;};explicit DiagnosticUartService(uint8_t value):value_(value),task_(SerialDirectTask::Kind::Write){}
 bool complete()const{return task_.stage()==SerialDirectTask::Stage::Complete;}const Effect&effect()const{return effect_;}uint8_t value()const{return value_;}uint8_t status()const{return task_.status();}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,SerialDirectTask>)return &task_;else return nullptr;}
 unsigned nextClocks(int previous=-1)const{return task_.nextClocks([&](SerialDirectTask::Write w){return byteClock(w==SerialDirectTask::Write::Data?0xffff8b:0xffff8c,previous);});}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;using T=SerialDirectTask;const auto stage=task_.stage();effect_={stage==T::Stage::Restore||stage==T::Stage::Finish,stage==T::Stage::Hold};return task_.advanceBefore(cost,[&](T::Read q){return q==T::Read::Input?value_:uint8_t(read(0xffff8c,cost));},[&](T::Write q,uint8_t v){write(q==T::Write::Data?0xffff8b:0xffff8c,v,cost);},[&](T::Write q){return byteClock(q==T::Write::Data?0xffff8b:0xffff8c,previous);});}
private:static unsigned byteClock(uint32_t a,int previous){return(a&1)&&previous==int(a-1)?0:1;}uint8_t value_;SerialDirectTask task_;Effect effect_{};
};
// Fixed-depth seven-word packing and actual direct UART output. Repeated
// groups reuse their semantic slot; output length never grows the owner.
class DiagnosticCodecService final {
public:
 using Kind=DiagnosticCodecTask::Kind;enum class Component{Codec,Serial};
 struct Input{uint32_t payload=0,source=0,sum=0;uint16_t weight=0,carry=0;};struct Effect{bool read=false,write=false;};
 struct Result{uint32_t source=0,sum=0,packed=0,payload=0,lastSumBefore=0;uint16_t weight=0,carry=0,sample=0;uint8_t byte=0;};
 explicit DiagnosticCodecService(Kind k):DiagnosticCodecService(k,Input{}){}
 DiagnosticCodecService(Kind k,Input i):kind_(k),input_(i){push(k,i);}
 Kind kind()const{return kind_;}const Input&input()const{return input_;}const Result&result()const{return result_;}const Effect&effect()const{return effect_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return !depth_;}bool critical()const{return false;}uint64_t bytesSent()const{return bytesSent_;}
 const DiagnosticUartService*serial()const{return serial_?&*serial_:nullptr;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,DiagnosticCodecService>)return this;else if constexpr(std::is_same_v<T,DiagnosticCodecTask>)return component_==Component::Codec&&depth_?&frames_[depth_-1]->task:nullptr;else if constexpr(std::is_same_v<T,DiagnosticUartService>)return component_==Component::Serial?serial():nullptr;else return component_==Component::Serial?serial_->template task<T>():nullptr;}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;return component_==Component::Serial?serial_->nextClocks(previous):frames_[depth_-1]->task.nextClocks(sampleCost(frames_[depth_-1]->task.source()));}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
 if(component_==Component::Serial){serial_->advanceBefore(cost,read,write,previous);effect_={serial_->effect().read,serial_->effect().write};if(serial_->complete()){++bytesSent_;frames_[depth_-1]->task.resumeChild(0,0,0,0);select(Component::Codec);}return cost;}
 using T=DiagnosticCodecTask;using S=T::Stage;auto&f=*frames_[depth_-1];auto&t=f.task;const auto stage=t.stage();if(stage==S::AddSampleChecksum)f.lastSumBefore=t.sum();
 t.advanceBefore(cost,[&](T::Read q,uint32_t raw)->uint16_t{if(q==T::Read::Prefix)return uint8_t(read(0x1b0+raw,cost-2));const auto a=raw&0xffffff;if(!readSpan(raw,2))throw std::logic_error("Unsupported diagnostic sample span");const auto back=a<0x400000&&!(a&1)?0:a>=0xfffd10&&a<0xffffa0&&!(a&1)?0:a>=0xffffa0?1:3;return uint16_t((uint16_t(read(a,cost-2-back))<<8)|uint8_t(read((a+1)&0xffffff,cost-2)));},[](auto,auto){},sampleCost(t.source()));
 effect_.write=stage==S::SaveLongPayload||stage==S::SaveLongHigh||stage==S::SendPrefixStart||stage==S::SendPrefixByte||stage==S::SendWordLow||stage==S::SendWordHigh||stage==S::SendLongLow||stage==S::SendLongHigh||stage==S::SendLongTop||stage==S::PackBlockGroup||stage==S::PackGroupSample||stage==S::SendPackedLow||stage==S::SendPackedHigh;
 effect_.read=stage==S::RestoreLongPayload||stage==S::RestoreLongHigh||stage==S::ReturnPrefix||stage==S::ReturnWord||stage==S::ReturnLong||stage==S::ReturnBlock||stage==S::ReturnPacked;
 if(t.awaitingChild()){
  if(stage==S::SendLongLow||stage==S::SendLongHigh){Input i;i.payload=t.word();push(Kind::Word,i);}
  else if(stage==S::PackBlockGroup){Input i;i.source=t.source();i.sum=t.sum();push(Kind::Group,i);}
  else if(stage==S::PackGroupSample){Input i;i.source=t.source();i.sum=t.sum();i.weight=t.weight();i.carry=t.carry();push(Kind::Sample,i);}
  else {serial_.emplace(stage==S::SendPrefixStart?0xf0:t.byte());select(Component::Serial);}
 }else if(t.complete()){
  Result r;r.source=t.source();r.sum=t.sum();r.packed=t.packed();r.payload=t.payload();r.weight=f.kind==Kind::Block?f.lastWeight:t.weight();r.carry=t.carry();r.sample=t.sample();r.lastSumBefore=f.lastSumBefore;r.byte=f.kind==Kind::Block?f.lastByte:t.byte();
  --depth_;if(depth_){auto&parent=*frames_[depth_-1];parent.lastWeight=r.weight;parent.lastByte=r.byte;parent.task.resumeChild(r.source,r.sum,r.carry,r.packed);select(Component::Codec);}else result_=r;
 }
 return cost;
 }
 static bool readSpan(uint32_t raw,unsigned n){const auto a=raw&0xffffff;return n&&((a>=0xb8e8&&a<0x200000&&n<=0x200000-a)||(a>=0x800000&&a<0x900000&&n<=0x900000-a)||(a>=0xa0b8e8&&a<0xc00000&&n<=0xc00000-a)||(a>=0xffdc00&&a<0xfff000&&n<=0xfff000-a));}
private:
 struct Frame{Kind kind;DiagnosticCodecTask task;uint32_t lastSumBefore=0;uint16_t lastWeight=0;uint8_t lastByte=0;Frame(Kind k,Input i):kind(k),task(k,i.payload,i.source,i.sum,i.weight,i.carry){}};
 static unsigned sampleCost(uint32_t raw){const auto a=raw&0xffffff;return a<0x400000?3+3*(a&1):a>=0xfffd10?(a>=0xffffa0?2:1+(a&1)):6;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}void push(Kind k,Input i){if(depth_==frames_.size())throw std::logic_error("Diagnostic codec depth exceeded");frames_[depth_++].emplace(k,i);select(Component::Codec);}
 Kind kind_;Input input_;Result result_{};Component component_=Component::Codec;Effect effect_{};uint32_t components_=1;unsigned depth_=0;uint64_t bytesSent_=0;std::array<std::optional<Frame>,4>frames_{};std::optional<DiagnosticUartService>serial_;
};
}
