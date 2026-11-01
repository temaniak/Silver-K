#pragma once
#include "kstationControllerDspTransferRuntime.h"
#include "kstationDspTriggerTask.h"
#include <array>
#include <variant>
namespace kstation {
// Note/reset DSP publication composed from domain tasks and the live bus.
// A caller's retained input interval is represented by its fixed timing only.
class ControllerDspTriggerRuntime final {
 using T=DspTriggerTask;using V=DspTriggerValueTask;using B=DspTriggerTableTask;
 using Job=std::variant<T,V,B>;
public:
 enum class Block:uint8_t{None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode,DspBusy};
 struct Result{unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
 bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(2))return false;for(auto a=0x8260u;a<0x833a;++a)if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;if(!d.protectImmutableVerified(0x8260,0x833a,image+0x8260)||!d.protectImmutableVerified(0x408260,0x40833a,image+0x8260))return false;bound_=&d;return true;}
 bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}
 bool start(uint8_t mode){if(!bound_||active())return false;jobs_[0].emplace<T>(mode);depth_=1;ownsDsp_=false;return true;}
 bool active()const{return depth_!=0;}void reset(){depth_=0;ownsDsp_=false;}
 unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq,const ControllerDspTransferRuntime&dsp)const{if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled)||(dsp.active()&&!ownsDsp_))return 0;if(ownsDsp_)return dsp.nextClocks(d,irq);constexpr DspTriggerCredits c;return std::visit([&](const auto&t){return t.nextClocks(c);},jobs_[depth_-1])+(d.pendingIrqs?1u:0u);}
 Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,ControllerDspTransferRuntime&dsp,unsigned available){
  if(bound_!=&d||!irq.boundTo(d)||!dsp.boundTo(d))return {0,false,Block::NotBound};if(!active())return {0,false,Block::Inactive};if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};if(dsp.active()&&!ownsDsp_)return {0,false,Block::DspBusy};
  auto&job=jobs_[depth_-1];
  if(ownsDsp_){auto result=dsp.advanceBefore(d,irq,available);if(result.returned){ownsDsp_=false;std::visit([](auto&t){using J=std::decay_t<decltype(t)>;if constexpr(!std::is_same_v<J,T>)t.resumeDsp();},job);}return {result.clocks,false,Block::None};}
  constexpr DspTriggerCredits credits;auto cost=std::visit([&](const auto&t){return t.nextClocks(credits);},job),arb=d.pendingIrqs?1u:0u;if(!cost||available<cost+arb)return {};
  if(arb)(void)d.read8(0xfffff2);auto begin=d.cycles;bool stackWrite=false;
  auto read=[&](uint32_t at,unsigned width,unsigned end){uint32_t value=0;auto total=width==4?6u:3u;for(unsigned i=0;i<width;++i){d.cycles=begin+end-total+(width==4&&i<2?3:total);d.lastRead=int(at+i);value=(value<<8)|d.readResource(at+i);}return value;};
  std::visit([&](auto&t){using J=std::decay_t<decltype(t)>;
   if constexpr(std::is_same_v<J,T>){using S=T::Stage;auto stage=t.stage();stackWrite=stage==S::RetainProgram||stage==S::RetainScratch||stage==S::RetainAddress||stage==S::RetainValue||stage==S::RetainPort||stage==S::SendSingle||stage==S::SendFirst||stage==S::SendSecond;
    auto used=t.advanceBefore(cost,credits,[&](T::Read field){auto offset=field==T::Read::Mode?0xc7:field==T::Read::Single?t.singleOffset():field==T::Read::First||field==T::Read::Second?0x4f:0x4e;return uint8_t(read(0xba5c+offset,1,cost));});if(used!=cost)throw std::logic_error("K DSP trigger root interval");
   }else if constexpr(std::is_same_v<J,V>){stackWrite=t.stage()==V::Stage::Send;if(t.advanceBefore(cost,credits)!=cost)throw std::logic_error("K DSP trigger value interval");}
   else {stackWrite=t.stage()==B::Stage::Send;if(t.advanceBefore(cost,credits,[&](uint32_t at){return read(at,4,cost);})!=cost)throw std::logic_error("K DSP trigger table interval");}
  },job);
  d.cycles=begin+cost;d.lastRead=-1;if(stackWrite)d.lastWrite=-1;
  if(auto*t=std::get_if<T>(&job);t&&t->awaitingChild()){if(depth_==jobs_.size())throw std::logic_error("K DSP trigger domain capacity");if(t->child()==T::Child::Value)jobs_[depth_++].emplace<V>(t->value(),t->address());else jobs_[depth_++].emplace<B>(t->value(),t->other(),t->address());}
  else if(std::visit([](const auto&t){using J=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<J,T>)return false;else return t.awaitingDsp();},job)){auto ok=std::visit([&](const auto&t){using J=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<J,T>)return false;else return dsp.startWrite(t.address(),t.word());},job);if(!ok)throw std::logic_error("K DSP trigger bus admission");ownsDsp_=true;}
  else if(std::visit([](const auto&t){return t.complete();},job)){--depth_;if(depth_&&!std::get<T>(jobs_[depth_-1]).resumeChild())throw std::logic_error("K DSP trigger root continuation");}
  return {cost+arb,!active(),Block::None};
 }
private:ControllerDeviceState*bound_=nullptr;std::array<Job,3>jobs_{};unsigned depth_=0;bool ownsDsp_=false;
};
}
