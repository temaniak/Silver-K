#pragma once
#include "ksClockSyncTask.h"
#include "ksControllerDspService.h"
#include <array>
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
// Live clock feedback, normalization and the three complete DSP transactions.
// The retained observations describe the algorithm, never an execution frame.
class ClockSyncService final {
public:
 enum class Kind { Update };
 enum class Component { Sync, Coefficient, PeriodAddress, PeriodValue };
 struct Input {};
 struct Effect { bool read=false,write=false; };
 struct Summary {
  uint8_t divider=0,source=0,sourceFlags=0,internalRate=0,firstActive=0,active=0;
  uint16_t signal=0,correctionGain=0,gain=0;
  uint32_t internalPeriod=0,period=0,synchronized=0,correction=0,normalized=0,coefficient=0,previousCoefficient=0,difference=0,publishPeriod=0,previousPeriod=0;
  bool internal=false,external=false,gainObserved=false,coefficientObserved=false,coefficientSent=false,periodSent=false;
 };
 explicit ClockSyncService(Kind k=Kind::Update):kind_(k){}
 ClockSyncService(Kind k,Input):ClockSyncService(k){}
 Kind kind()const{return kind_;}Component component()const{return component_;}
 uint32_t components()const{return components_;}bool complete()const{return complete_;}bool critical()const{return false;}
 const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}
 const ClockSyncTask&syncTask()const{return sync_;}
 const ControllerDspService*transaction(Component c)const {const auto n=unsigned(c);return n&&n<=3&&finished_[n-1]?&*finished_[n-1]:nullptr;}
 template<class T>const T*task()const {
  if constexpr(std::is_same_v<T,ClockSyncTask>)return component_==Component::Sync?&sync_:nullptr;
  else if constexpr(std::is_same_v<T,ControllerDspService>)return dsp_?&*dsp_:nullptr;
  else return dsp_?dsp_->template task<T>():nullptr;
 }
 static bool endpoint(uint32_t raw){const auto a=raw&0xffffff;return (a>=0xffee7e&&a<=0xffeea5)||a==0x169ef||a==0x16950||(a>=0x159f2&&a<=0x159f3)||(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffec22||a==0xffff61||a==0xffff62;}
 unsigned nextClocks(int previousWrite=-1)const{return complete_?0:dsp_?dsp_->nextClocks(previousWrite):sync_.nextClocks();}
 template<class Read,class Write>unsigned advanceBefore(unsigned budget,Read&&read,Write&&write,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||budget<cost)return 0;effect_={};
  const auto r=[&](uint32_t a,unsigned dt){if(!endpoint(a))throw std::logic_error("Unsupported clock-sync read resource");return uint8_t(read(a,dt));};
  const auto w=[&](uint32_t a,uint8_t v,unsigned dt){if(!endpoint(a))throw std::logic_error("Unsupported clock-sync write resource");write(a,v,dt);};
  if(dsp_){dsp_->advanceBefore(cost,r,w,previousWrite);effect_={dsp_->effect().read,dsp_->effect().write};if(dsp_->complete()){finished_[unsigned(component_)-1]=*dsp_;dsp_.reset();sync_.resumeChild();select(Component::Sync);}return cost;}
  using T=ClockSyncTask;using S=T::Stage;const auto stage=sync_.stage();
  const auto wide=[&](uint32_t a,unsigned bytes){uint32_t v=0;for(unsigned n=0;n<bytes;++n)v=(v<<8)|r(a+n,cost-3*(bytes-1-n));return v;};
  sync_.advanceBefore(cost,[&](T::Read field)->uint32_t{
   switch(field){
   case T::Read::Divider:return r(0xffee98,cost);
   case T::Read::Source:return summary_.source=r(0x169ef,cost);
   case T::Read::Flags:return summary_.sourceFlags=r(0xffee97,(stage==S::ClearSourceFlag||stage==S::WriteSourceFlag)?cost-3:cost);
   case T::Read::InternalRate:summary_.internal=true;return summary_.internalRate=r(0x16950,cost);
   case T::Read::Period:if(stage==S::ReadPublishPeriod)return summary_.publishPeriod=wide(0xffee7e,4);return summary_.period=wide(0xffee7e,4);
   case T::Read::Active:if(stage==S::ReadActiveAgain)return summary_.active=r(0xffee96,cost);return summary_.firstActive=r(0xffee96,cost);
   case T::Read::Signal:summary_.external=true;return summary_.signal=uint16_t(wide(0xffeea0,2));
   case T::Read::Synchronized:return summary_.synchronized=wide(0xffee8e,4);
   case T::Read::Gain:if(stage==S::ReadGainAgain){summary_.gainObserved=true;return summary_.gain=uint16_t(wide(0xffeea4,2));}return summary_.correctionGain=uint16_t(wide(0xffeea4,2));
   case T::Read::Coefficient:summary_.coefficientObserved=true;return summary_.previousCoefficient=wide(0xffee92,4);
   case T::Read::PreviousPeriod:return summary_.previousPeriod=wide(0xffee8a,4);
   }throw std::logic_error("Invalid clock-sync observation");
  },[&](T::Write field,uint32_t value){uint32_t a=0;unsigned bytes=1;switch(field){
   case T::Write::Divider:a=0xffee98;summary_.divider=uint8_t(value);break;
   case T::Write::Flags:a=0xffee97;break;
   case T::Write::Active:a=0xffee96;break;
   case T::Write::Period:a=0xffee7e;bytes=4;summary_.internalPeriod=value;break;
   case T::Write::Synchronized:a=0xffee8e;bytes=4;break;
   case T::Write::Gain:a=0xffeea4;bytes=2;break;
   case T::Write::Coefficient:a=0xffee92;bytes=4;break;
   case T::Write::PreviousPeriod:a=0xffee8a;bytes=4;break;
   }for(unsigned n=0;n<bytes;++n)w(a+n,uint8_t(value>>(8*(bytes-1-n))),cost-3*(bytes-1-n));
  });
  if(stage==S::DoubleCorrection)summary_.correction=uint32_t(int32_t(int16_t(summary_.signal-uint16_t(summary_.synchronized>>16)))*int32_t(int16_t(summary_.correctionGain)))<<1;
  if(stage==S::DivideHigh)summary_.normalized=sync_.rate();
  if(stage==S::CombineCoefficient)summary_.coefficient=sync_.coefficient();
  if(stage==S::DifferenceCoefficient){summary_.difference=summary_.previousCoefficient-summary_.coefficient;if(summary_.difference&0x80000000u)summary_.difference=0u-summary_.difference;}
  effect_.write=stage==S::RetainPeriodWord||stage==S::RetainPeriodValue;
  effect_.read=stage==S::RestorePeriodValue||stage==S::RestorePeriodWord||stage==S::ReturnIdle||stage==S::ReturnUpdate;
  if(sync_.awaitingChild()){
   effect_.write=true;Component c;DspPort port;uint16_t address;uint32_t value;
   if(stage==S::SendCoefficient){c=Component::Coefficient;port=DspPort::A;address=4;value=sync_.coefficient();summary_.coefficientSent=true;}
   else{c=stage==S::SendPeriodAddress?Component::PeriodAddress:Component::PeriodValue;port=c==Component::PeriodAddress?DspPort::B:DspPort::A;address=3;value=sync_.period();summary_.periodSent=true;}
   dsp_.emplace(ControllerDspService::Kind::Routing,ControllerDspService::Input{port==DspPort::A?DspRoutingTask::Kind::WriteA:DspRoutingTask::Kind::WriteB,port,address,value>>8});select(c);
  }
  if(sync_.complete())complete_=true;return cost;
 }
 template<class Read,class Write,class Resolver>unsigned advanceBefore(unsigned budget,Read&&r,Write&&w,Resolver&&,int previousWrite=-1){return advanceBefore(budget,r,w,previousWrite);}
private:
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 Kind kind_;Component component_=Component::Sync;uint32_t components_=1;bool complete_=false;Effect effect_{};Summary summary_{};ClockSyncTask sync_{ClockSyncTask::Kind::Update};std::optional<ControllerDspService>dsp_;std::array<std::optional<ControllerDspService>,3>finished_{};
};
}
