#pragma once
#include "ksPedalSenseTask.h"
#include "ksPanelEncoderTask.h"
#include "ksPanelAdcScanTask.h"
#include <optional>
namespace ks {
// Closed normal panel-control work. ADC warmup/initialization is deliberately
// outside these seven roots; Start owns its complete Mux child.
class PanelControlsService final {
public:
 enum class Kind { Switches, PedalReset, Pedal, Encoder, AdcStart, AdcMux, AdcResetLast };
 enum class Component { Switches, PedalReset, Pedal, Encoder, AdcStart, AdcMux, AdcResetLast };
 explicit PanelControlsService(Kind kind):kind_(kind),active_(static_cast<Component>(kind)),components_(1u<<unsigned(kind)){
  if(kind<=Kind::Pedal)pedal_.emplace(kind==Kind::Switches?PedalSenseTask::Kind::Switches:kind==Kind::PedalReset?PedalSenseTask::Kind::Reset:PedalSenseTask::Kind::Pedal);
  else if(kind==Kind::Encoder)encoder_.emplace();
  else adc_.emplace(kind==Kind::AdcStart?PanelAdcScanTask::Kind::Start:kind==Kind::AdcMux?PanelAdcScanTask::Kind::Mux:PanelAdcScanTask::Kind::ResetLast);
 }
 Kind kind()const{return kind_;}Component component()const{return active_;}uint32_t components()const{return components_;}bool complete()const{return complete_;}
 uint8_t switches()const{return pedal_->switches();}uint8_t pedalResult()const{return pedal_->result();}bool pedalCarry()const{return bool(pedal_->polarity()&128);}
 uint8_t quarter()const{return uint8_t(encoder_->timer()>>2);}uint8_t delta()const{return uint8_t(encoder_->previousQuarter()-quarter());}
 uint8_t previousQuarter()const{return encoder_->previousQuarter();}uint8_t previousDelta()const{return encoder_->previousDelta();}
 uint8_t encoderGain()const{return delta()?encoder_->result():uint8_t((encoder_->activity()>4?encoder_->activity()-4:0)+1);}
 const PanelAdcScanTask* adcTask()const{return active_==Component::AdcMux&&kind_==Kind::AdcStart?(mux_?&*mux_:nullptr):(adc_?&*adc_:nullptr);}
 const PanelEncoderTask* encoderTask()const{return encoder_?&*encoder_:nullptr;}
 uint8_t initialAdcGroup()const{return adc_->group();}bool usedMux()const{return kind_==Kind::AdcStart&&mux_.has_value();}
 uint8_t muxGroup()const{return kind_==Kind::AdcMux?adc_->group():mux_->group();}
 unsigned nextClocks()const{
  if(complete_)return 0;if(active_<=Component::Pedal)return pedal_->nextClocks();if(active_==Component::Encoder)return encoder_->nextClocks();
  return active_==Component::AdcMux&&kind_==Kind::AdcStart?mux_->nextClocks():adc_->nextClocks();
 }
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){
  const auto cost=nextClocks();if(!cost||cost>available)return 0;
  if(active_<=Component::Pedal){
   using T=PedalSenseTask;pedal_->advanceBefore(cost,[&](T::Read field){return read(field==T::Read::Switches?0xffff53:field==T::Read::Polarity?0x169fa:field==T::Read::Port?0xffff50:0x1537a,cost);},[&](uint8_t value){write(0x1537a,value,cost);});complete_=pedal_->complete();
  }else if(active_==Component::Encoder){
   using T=PanelEncoderTask;encoder_->advanceBefore(cost,[&](T::Read field)->uint16_t{
    if(field==T::Read::Timer){const auto high=uint8_t(read(0xfffff6,cost-1));return uint16_t((unsigned(high)<<8)|uint8_t(read(0xfffff7,cost)));}
    return read(field==T::Read::HalfHistory?0xffee73:field==T::Read::Activity?0xffee74:field==T::Read::QuarterHistory?0xffee72:0xffee75,cost);
   },[&](T::Write field,uint8_t value){write(field==T::Write::HalfHistory?0xffee73:field==T::Write::Activity?0xffee74:field==T::Write::QuarterHistory?0xffee72:0xffee75,value,cost);});complete_=encoder_->complete();
  }else{
   using T=PanelAdcScanTask;const bool child=active_==Component::AdcMux&&kind_==Kind::AdcStart;auto&task=child?*mux_:*adc_;
   task.advanceBefore(cost,[&](T::Read){return read(0x156e2,cost);},[&](T::Write field,uint8_t value){write(field==T::Write::Group?0x156e2:field==T::Write::Last?0x156e4:field==T::Write::Status?0xffff98:field==T::Write::Mux?0xffff6e:0x156e3,value,cost);});
   if(task.awaitingChild()){mux_.emplace(T::Kind::Mux);select(Component::AdcMux);}
   else if(task.complete()){if(child){adc_->resumeChild();select(Component::AdcStart);}else complete_=true;}
  }
  return cost;
 }
private:
 void select(Component component){active_=component;components_|=1u<<unsigned(component);}
 Kind kind_;Component active_;uint32_t components_;bool complete_=false;
 std::optional<PedalSenseTask> pedal_;std::optional<PanelEncoderTask> encoder_;std::optional<PanelAdcScanTask> adc_,mux_;
};
}
