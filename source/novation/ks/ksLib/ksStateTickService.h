#pragma once
#include "ksStateForegroundTask.h"
#include "ksTimerServiceTask.h"
#include "ksPanelScanService.h"
#include "ksPanelControlsService.h"
#include "ksStateEncoderService.h"
#include "ksPanelPedalOutputService.h"
#include "ksStateRefreshService.h"
#include <optional>
#include <type_traits>
#include <stdexcept>
namespace ks {
// Complete periodic panel transaction. Captured ticks, physical inputs and
// typed child outcomes survive interrupts without an instruction frame.
class StateTickService final {
public:
 enum class Kind { Tick };
 enum class Component { Tick, Clock, Buttons, Adc, Encoder, Pedal, Output, Refresh };
 struct Input {};
 struct Effect { bool read=false,write=false; };
 struct Summary { uint32_t counter=0;uint16_t tick=0,last=0,elapsed=0,presentation=0;uint8_t pedal=128;bool due=false,sent=false; };
 StateTickService()=default;
 explicit StateTickService(Kind):StateTickService(){}
 StateTickService(Kind,Input):StateTickService(){}
 Kind kind()const{return Kind::Tick;}bool complete()const{return parent_.complete();}
 Component component()const{return component_;}uint32_t components()const{return components_;}
 const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}
 bool critical()const{if(component_==Component::Encoder)return encoder_->critical();if(component_==Component::Output)return output_->critical();if(component_==Component::Refresh)return refresh_->critical();return false;}
 const PanelScanService*buttons()const{return buttons_?&*buttons_:nullptr;}
 const PanelControlsService*adc()const{return adc_?&*adc_:nullptr;}
 const StateEncoderService*encoder()const{return encoder_?&*encoder_:nullptr;}
 const PanelControlsService*pedal()const{return pedal_?&*pedal_:nullptr;}
 const PanelPedalOutputService*output()const{return output_?&*output_:nullptr;}
 const StateRefreshService*refresh()const{return refresh_?&*refresh_:nullptr;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,StateTickService>)return this;
  else if constexpr(std::is_same_v<T,ControllerClockReadTask>)return component_==Component::Clock?&clock_:nullptr;
  else if constexpr(std::is_same_v<T,PanelScanService>)return component_==Component::Buttons?buttons():nullptr;
  else if constexpr(std::is_same_v<T,PanelControlsService>)return component_==Component::Adc?adc():component_==Component::Pedal?pedal():component_==Component::Encoder?encoder_->template task<T>():nullptr;
  else if constexpr(std::is_same_v<T,PanelAdcScanTask>)return component_==Component::Adc?adc_->adcTask():nullptr;
  else if constexpr(std::is_same_v<T,PedalSenseTask>)return component_==Component::Pedal?pedal_->pedalTask():nullptr;
  else if constexpr(std::is_same_v<T,StateEncoderService>)return component_==Component::Encoder?encoder():nullptr;
  else if constexpr(std::is_same_v<T,PanelPedalOutputService>)return component_==Component::Output?output():nullptr;
  else if constexpr(std::is_same_v<T,StateRefreshService>)return component_==Component::Refresh?refresh():nullptr;
  else {
   if constexpr(std::is_same_v<T,StateForegroundTask>)if(component_==Component::Tick)return &parent_;
   if(component_==Component::Encoder)return encoder_->template task<T>();
   if(component_==Component::Output)return output_->template task<T>();
   if(component_==Component::Refresh)return refresh_->template task<T>();
   return nullptr;
  }
 }
 template<class Peek,class Resolver>static bool admitted(Peek&&peek,Resolver&&resolver){
  const auto word=[&](unsigned a){return (unsigned(uint8_t(peek(a)))<<8)|uint8_t(peek(a+1));};
  return !(word(0x1537c)&0xff81)&&!(word(0x1537e)&0xff81)&&word(0x15b04)<256&&word(0x15b06)<256&&!uint8_t(peek(0xffed6c))&&!uint8_t(peek(0xffed6e))&&StateRefreshService::admitted(peek,[&](uint8_t c,uint32_t p){return resolver.schema(c,p);});
 }
 unsigned nextClocks(int previous=-1)const{
  if(complete())return 0;
  switch(component_){case Component::Tick:return parent_.nextClocks();case Component::Clock:return clock_.nextClocks();case Component::Buttons:return buttons_->nextClocks();case Component::Adc:return adc_->nextClocks();case Component::Encoder:return encoder_->nextClocks(previous);case Component::Pedal:return pedal_->nextClocks();case Component::Output:return output_->nextClocks(previous);case Component::Refresh:return refresh_->nextClocks(previous);}return 0;
 }
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolver,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  if(component_==Component::Tick){using T=StateForegroundTask;using S=T::Stage;const auto stage=parent_.stage();
   parent_.advanceBefore(cost,[&](T::Read field)->uint16_t{if(field!=T::Read::LastTick)throw std::logic_error("Unexpected tick read");const auto high=uint8_t(read(0x16780,cost));return summary_.last=uint16_t((unsigned(high)<<8)|uint8_t(read(0x16781,cost)));},[&](T::Write field,uint16_t value){if(field!=T::Write::LastTick)throw std::logic_error("Unexpected tick write");write(0x16780,uint8_t(value>>8),cost);write(0x16781,uint8_t(value),cost);});
   if(stage==S::RetainTick)summary_.tick=uint16_t(summary_.counter);
   if(stage==S::ElapsedTick)summary_.elapsed=uint16_t(summary_.tick-summary_.last);
   if(stage==S::SelectTick)summary_.due=summary_.elapsed>=25;
   if(stage==S::ReturnTick)effect_.read=true;
   if(parent_.awaitingChild()){
    effect_.write=true;
    switch(stage){case S::ReadTick:select(Component::Clock);break;
     case S::ScanButtons:buttons_.emplace(PanelScanService::Kind::Scan);select(Component::Buttons);break;
     case S::StartAdc:adc_.emplace(PanelControlsService::Kind::AdcStart);select(Component::Adc);break;
     case S::ScanEncoder:encoder_.emplace(StateEncoderService::Kind::Encoder,StateEncoderService::Input{summary_.elapsed});select(Component::Encoder);break;
     case S::ScanPedal:pedal_.emplace(PanelControlsService::Kind::Pedal);select(Component::Pedal);break;
     case S::SendPedal:summary_.sent=true;output_.emplace(PanelPedalOutputService::Kind::Pedal,PanelPedalOutputService::Input{summary_.presentation});select(Component::Output);break;
     case S::RefreshPanel:refresh_.emplace();select(Component::Refresh);break;
     default:throw std::logic_error("Unexpected tick child");}
   }
  }else if(component_==Component::Clock){const auto stage=clock_.stage();clock_.advanceBefore(cost,[&](){uint32_t value=0;for(unsigned n=0;n<4;++n)value=(value<<8)|uint8_t(read(0xffec10+n,9+3*n));return summary_.counter=value;});effect_.read=stage==ControllerClockReadTask::Stage::Return;if(clock_.complete())resume(uint16_t(summary_.counter));
  }else if(component_==Component::Buttons){matrixEffect();buttons_->advanceBefore(cost,[&](uint32_t a,unsigned dt){const auto v=uint8_t(read(a,dt));if((a==0x1537c||a==0x1537e)&&v)throw std::logic_error("Invalid live panel producer high byte");if((a==0x1537d||a==0x1537f)&&(v&0x81))throw std::logic_error("Invalid live panel producer low byte");return v;},write);if(buttons_->complete())resume(summary_.elapsed);
  }else if(component_==Component::Adc){const auto stage=adc_->adcTask()->stage();using S=PanelAdcScanTask::Stage;effect_.write=stage==S::SaveStartBase||stage==S::SaveStartAux||stage==S::SelectMux;effect_.read=stage==S::RestoreStartAux||stage==S::RestoreStartBase||stage==S::ReturnStart||stage==S::ReturnMux;adc_->advanceBefore(cost,read,write);if(adc_->complete())resume(summary_.elapsed);
  }else if(component_==Component::Encoder){encoder_->advanceBefore(cost,read,write,resolver,previous);effect_={encoder_->effect().read,encoder_->effect().write};if(encoder_->complete()){summary_.presentation=encoder_->presentation();resume(summary_.presentation);}
  }else if(component_==Component::Pedal){using S=PedalSenseTask::Stage;const auto stage=pedal_->pedalTask()->stage();effect_.write=stage==S::SaveScratch;effect_.read=stage==S::RestoreScratch||stage==S::ReturnPedal;pedal_->advanceBefore(cost,read,write);if(pedal_->complete()){summary_.pedal=pedal_->pedalResult();summary_.presentation=uint16_t((unsigned(summary_.pedal)<<8)|(summary_.presentation&255));resume(summary_.presentation);}
  }else if(component_==Component::Output){output_->advanceBefore(cost,read,write,previous);effect_={output_->effect().read,output_->effect().write};if(output_->complete())resume(output_->summary().event);
  }else {refresh_->advanceBefore(cost,read,write,resolver,previous);effect_={refresh_->effect().read,refresh_->effect().write};if(refresh_->complete())resume(0);}
  return cost;
 }
private:
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void resume(uint16_t event){if(!parent_.resumeChild(event,false))throw std::logic_error("Invalid tick continuation");select(Component::Tick);}
 void matrixEffect(){using M=PanelMatrixTask::Stage;using P=PanelInputTask::Stage;
  for(auto s:{M::SaveScratch,M::SaveMux,M::SaveBits,M::SaveIndex,M::SaveTarget,M::SaveEvent,M::SaveDelay,M::PublishEvent})if(buttons_->atMatrix(s))effect_.write=true;
  for(auto s:{M::RestoreDelay,M::RestoreSavedEvent,M::RestoreTarget,M::RestoreIndex,M::RestoreBits,M::RestoreMux,M::RestoreScratch,M::ReturnScan})if(buttons_->atMatrix(s))effect_.read=true;
  if(buttons_->atPush(P::ReturnPush))effect_.read=true;
 }
 StateForegroundTask parent_{StateForegroundTask::Kind::Tick};ControllerClockReadTask clock_;
 Component component_=Component::Tick;uint32_t components_=1;Summary summary_{};Effect effect_{};
 std::optional<PanelScanService>buttons_;std::optional<PanelControlsService>adc_,pedal_;
 std::optional<StateEncoderService>encoder_;std::optional<PanelPedalOutputService>output_;std::optional<StateRefreshService>refresh_;
};
}
