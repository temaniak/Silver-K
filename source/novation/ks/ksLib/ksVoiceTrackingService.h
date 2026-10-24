#pragma once
#include "ksVoiceTrackingTask.h"
#include "ksControllerDspService.h"
#include "ksControllerCoefficientService.h"
#include "ksStateStoreService.h"
#include <array>
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
class VoiceTrackingService final {
public:
 enum class Component{Tracking,Dsp,Coefficient};struct Effect{bool read=false,write=false;};
 struct Summary{VoiceTrackingTask tracking{VoiceTrackingTask::Kind::Refresh};std::array<std::optional<VoiceTrackingTask>,2>effects{};uint32_t patch=0;DspPort port=DspPort::A;uint16_t address=0,transportRemaining=0;bool patchObserved=false,transportKnown=false;std::optional<ControllerDspService>lastDsp;};
 VoiceTrackingService():task_(VoiceTrackingTask::Kind::Refresh){}
 bool complete()const{return complete_;}Component component()const{return dsp_?Component::Dsp:coefficient_?Component::Coefficient:Component::Tracking;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,VoiceTrackingTask>)return &task_;else if constexpr(std::is_same_v<T,ControllerDspService>)return dsp_?&*dsp_:nullptr;else if constexpr(std::is_same_v<T,ControllerCoefficientService>)return coefficient_?&*coefficient_:nullptr;else return nullptr;}
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;if(dsp_)return dsp_->nextClocks(previous);if(coefficient_)return coefficient_->nextClocks();return task_.complete()?17:task_.nextClocks(task_.publication()?byteClock(0x174a7,previous):3);}
 template<class R,class W>unsigned advanceBefore(unsigned available,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||available<cost)return 0;effect_={};
  if(dsp_){dsp_->advanceBefore(cost,read,write,previous);effect_={dsp_->effect().read,dsp_->effect().write};if(dsp_->complete()){summary_.port=dsp_->port();summary_.address=dsp_->address();if(dsp_->summary().clearedRetry){summary_.transportKnown=true;summary_.transportRemaining=0;}if(dsp_->summary().retried){summary_.transportKnown=true;summary_.transportRemaining=dsp_->summary().remaining;}summary_.lastDsp=dsp_;dsp_.reset();task_.resumeChild();}return cost;}
  if(coefficient_){coefficient_->advanceBefore(cost);effect_={coefficient_->effect().read,coefficient_->effect().write};if(coefficient_->complete()){const auto value=coefficient_->value();coefficient_.reset();task_.resumeChild(uint32_t(value));}return cost;}
  using T=VoiceTrackingTask;using S=T::Stage;if(task_.complete()){effect_.read=true;summary_.tracking=task_;complete_=true;return cost;}const auto stage=task_.stage();const auto effect=task_.effect();
  const auto byte=[&](uint32_t raw){if(!dataSpan(raw,1))throw std::logic_error("Unsupported evolving voice tracking byte");return uint8_t(read(raw&0xffffff,cost));};
  const auto wide=[&](uint32_t raw,unsigned n){if(!dataSpan(raw,n))throw std::logic_error("Unsupported evolving voice tracking span");uint32_t v=0;for(unsigned j=0;j<n;++j)v=(v<<8)|uint8_t(read((raw+j)&0xffffff,cost-3*((n-1-j)/2)));return v;};
  task_.advanceBefore(cost,[&](T::Read r)->uint32_t{switch(r){case T::Read::Part:return byte(0x17526);case T::Read::ActivePart:return byte(0x16798);case T::Read::Tracked:return byte(0x1755e);case T::Read::PatchReference:summary_.patch=wide(0x167bc,4);if(!dataSpan(summary_.patch,77))throw std::logic_error("Unsupported evolving tracked patch");summary_.patchObserved=true;return 0;case T::Read::Sample:return byte(summary_.patch+(effect?76:73));case T::Read::Time:return byte(summary_.patch+(effect?75:72));case T::Read::Base:return wide(0x15138,2);case T::Read::Table:return wide(0x159f4+task_.sampleIndex(),4);default:throw std::logic_error("Unsupported tracking observation");}},[&](T::Write,uint32_t v){write(0x174a7,uint8_t(v),cost);},[&](T::Write){return byteClock(0x174a7,previous);});
  if(stage==S::RetainTime)effect_.write=true;if(stage==S::RestoreTime)effect_.read=true;
  if(stage==S::ClearLowHigh){summary_.transportKnown=true;summary_.transportRemaining=0;}
  if(stage==S::DispatchSecond)summary_.effects[effect]=task_;
  if(task_.child()!=T::Child::None)effect_.write=true;
  switch(task_.child()){case T::Child::None:break;case T::Child::Select:startDsp(DspRoutingTask::Kind::VoiceB,0,0);break;case T::Child::First:startDsp(DspRoutingTask::Kind::WriteRelative,task_.firstOffset(),task_.packed()>>8);break;case T::Child::Second:startDsp(DspRoutingTask::Kind::WriteRelative,task_.secondOffset(),task_.packed()>>8);break;case T::Child::Coefficient:coefficient_.emplace(int16_t(task_.rate()));break;default:throw std::logic_error("Unsupported tracking child");}
  return cost;
 }
 static bool dataSpan(uint32_t raw,unsigned n){const auto a=raw&0xffffff;return StateStoreService::dataSpan(raw,n)||(n&&a>=0x800000&&a<0x900000&&n<=0x900000-a);}
private:
 static unsigned byteClock(uint32_t a,int p){return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 void startDsp(DspRoutingTask::Kind route,uint16_t address,uint32_t word){ControllerDspService::Input in;in.routing=route;in.port=summary_.port;in.address=address;in.word=word;dsp_.emplace(ControllerDspService::Kind::Routing,in);}
 VoiceTrackingTask task_;Summary summary_{};Effect effect_{};bool complete_=false;std::optional<ControllerDspService>dsp_;std::optional<ControllerCoefficientService>coefficient_;
};
}
