#pragma once
#include "ksPanelSettingsLcdService.h"
#include "ksPanelVersionTask.h"
#include "ksPanelPercentTask.h"
#include "ksClockDisplayTask.h"
#include "ksSystemServicesTask.h"
#include <array>
#include <optional>
#include <type_traits>
namespace ks {
// Complete status presentation, including the actual LCD protocol. Only
// observed version, clock, level and presentation values survive suspension.
class PanelMonitorService final {
public:
 enum class Kind{Clock,Bar,Version,WithPatch,Patch,Percent};
 enum class Component{Clock,Bar,Version,Percent,Lcd};
 struct Input{uint32_t source=0;uint16_t presentation=0,numerator=0,divisor=0;uint8_t level=0,patch=0;};
 struct Effect{bool read=false,write=false;};
 struct Summary{uint16_t presentation=0;uint8_t barLevel=0,barRemainder=0;bool barPartial=false;uint8_t primary=0,secondary=0;uint32_t versionSource=0;uint16_t percentDivisor=0;uint32_t percentResult=0;uint8_t clockIndex=0,clockFlags=0;uint32_t clockPeriod=0;uint16_t clockNumber=0;StateDisplayService::LcdResult lcd{};};
 explicit PanelMonitorService(Kind k):PanelMonitorService(k,Input{}){}
 PanelMonitorService(Kind k,Input i):kind_(k),input_(i),presentation_(i.presentation){summary_.presentation=i.presentation;
  if(k==Kind::Clock){clock_.emplace(ClockDisplayTask::Kind::Display);select(Component::Clock);}
  else if(k==Kind::Bar){bar_.emplace(SystemServicesTask::Kind::Bar,i.level);select(Component::Bar);}
  else if(k==Kind::Percent){percent_.emplace();select(Component::Percent);}
  else startVersion(k==Kind::Version?PanelVersionTask::Kind::Version:k==Kind::WithPatch?PanelVersionTask::Kind::WithPatch:PanelVersionTask::Kind::Patch);
 }
 Kind kind()const{return kind_;}const Input&input()const{return input_;}const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return complete_;}bool critical()const{return false;}uint16_t presentation()const{return presentation_;}
 const PanelSettingsLcdService*lastLcd()const{return lcd_?&*lcd_:nullptr;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,PanelMonitorService>)return this;else if constexpr(std::is_same_v<T,ClockDisplayTask>)return component_==Component::Clock?&*clock_:nullptr;else if constexpr(std::is_same_v<T,SystemServicesTask>)return component_==Component::Bar?&*bar_:nullptr;else if constexpr(std::is_same_v<T,PanelVersionTask>)return component_==Component::Version&&depth_?&*versions_[depth_-1]:nullptr;else if constexpr(std::is_same_v<T,PanelPercentTask>)return component_==Component::Percent?&*percent_:nullptr;else if constexpr(std::is_same_v<T,PanelSettingsLcdService>)return component_==Component::Lcd?&*lcd_:nullptr;else return nullptr;}
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;switch(component_){case Component::Clock:return clock_->nextClocks();case Component::Bar:return bar_->nextClocks();case Component::Version:return versions_[depth_-1]->nextClocks();case Component::Percent:return percent_->nextClocks();case Component::Lcd:return lcd_->nextClocks(previous);}return 0;}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
 const auto byte=[&](uint32_t a,unsigned back=0){return uint8_t(read(a&0xffffff,cost-back));};
 const auto wide=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|byte(a+n,3*(3-n));return v;};
 switch(component_){
 case Component::Lcd:{lcd_->advanceBefore(cost,read,write,previous);effect_={lcd_->effect().read,lcd_->effect().write};if(lcd_->complete()){presentation_=lcd_->output();summary_.lcd=lcd_->lcdResult();if(lcdOwner_==Component::Clock){clock_->resumeChild();select(Component::Clock);}else if(lcdOwner_==Component::Bar){if(barTail_){complete_=true;}else {bar_->resumeChild();select(Component::Bar);}}else if(lcdOwner_==Component::Percent){percent_->resumeChild();select(Component::Percent);}else {versions_[depth_-1]->resumeChild();select(Component::Version);}}break;}
 case Component::Clock:{using T=ClockDisplayTask;using S=T::Stage;const auto stage=clock_->stage();clock_->advanceBefore(cost,[&](T::Read q)->uint32_t{return q==T::Read::Active?byte(0xffee96):q==T::Read::Flags?byte(0xffee97):wide(0xffee7e);});
  switch(stage){case S::SaveDisplayValue:case S::SaveDisplayIndex:case S::SaveDisplayText:effect_.write=true;break;case S::RestoreDisplayText:presentation_=input_.presentation;effect_.read=true;break;case S::RestoreDisplayIndex:case S::RestoreDisplayValue:case S::ReturnDisplay:effect_.read=true;break;case S::BindDisplayPosition:setByte(0x8d);break;case S::BindNumberPosition:setByte(32);break;default:break;}
  if(clock_->awaitingChild()){effect_.write=true;if(stage==S::DisplayText){presentation_=uint16_t(0x2a0c+clock_->index());startLcd(PanelSettingsLcdService::Kind::Text,0,0x2a0c+clock_->index());}else if(stage==S::DisplayNumber)startLcd(PanelSettingsLcdService::Kind::Number,clock_->number());else startLcd(PanelSettingsLcdService::Kind::Character);}
  if(clock_->complete()){summary_.clockIndex=clock_->index();summary_.clockFlags=clock_->flags();summary_.clockPeriod=clock_->period();summary_.clockNumber=clock_->number();complete_=true;}break;}
 case Component::Bar:{using T=SystemServicesTask;using S=T::Stage;const auto stage=bar_->stage();bar_->advanceBefore(cost,[](auto,auto){return 0u;},[](auto,auto){});
  switch(stage){case S::BindBarPosition:setByte(0x80);break;case S::ClampBarValue:summary_.barLevel=25;break;case S::RetainBarValue:summary_.barLevel=input_.level;break;case S::BindFullBar:setByte(5);break;case S::BindBarCount:presentation_=uint16_t(0x500|uint8_t(presentation_));break;case S::DecrementSolidCount:case S::DecrementPartialCount:case S::DecrementBlankCount:presentation_=uint16_t((presentation_&255)|uint16_t(uint8_t((presentation_>>8)-1))<<8);break;case S::RestorePartialBar:summary_.barPartial=true;summary_.barRemainder=bar_->value();break;case S::BindPartialBar:setByte(bar_->value());break;case S::BindBlankBar:setByte(0);break;case S::BindBarEnd:setByte(6);break;default:break;}
  if(bar_->awaitingChild()){effect_.write=true;startLcd(stage==S::SetBarPosition?PanelSettingsLcdService::Kind::Character:PanelSettingsLcdService::Kind::Raw);}
  else if(stage==S::CloseBar){barTail_=true;startLcd(PanelSettingsLcdService::Kind::Character);}break;}
 case Component::Version:{using T=PanelVersionTask;using S=T::Stage;auto&t=*versions_[depth_-1];const auto stage=t.stage();t.advanceBefore(cost,[&](T::Read q)->uint32_t{if(q==T::Read::Source)return input_.source;if(q==T::Read::Primary)return byte(t.source(),2);if(q==T::Read::Secondary)return byte(t.source()+1);return kind_==Kind::Patch?input_.patch:summary_.secondary;});
  if(stage==S::ReadPrimary){summary_.primary=t.primary();summary_.versionSource=t.source();presentation_=uint16_t(t.source()+1);}if(stage==S::ReadSecondary)summary_.secondary=t.secondary();
  if(stage==S::SetDot||stage==S::SetPatchDot)setByte('.');if(stage==S::AsciiMinor)setByte(uint8_t('0'+t.minor()));
  if(t.child()!=T::Child::None){effect_.write=true;if(t.child()==T::Child::Version)startVersion(T::Kind::Version);else if(t.child()==T::Child::Major)startLcd(PanelSettingsLcdService::Kind::TwoSpaces,t.major());else if(t.child()==T::Child::PatchValue)startLcd(PanelSettingsLcdService::Kind::TwoZeros,t.patch());else startLcd(PanelSettingsLcdService::Kind::Character);}
  else if(t.stage()==S::Complete){effect_.read=true;--depth_;if(depth_){versions_[depth_-1]->resumeChild();select(Component::Version);}else complete_=true;}break;}
 case Component::Percent:{using T=PanelPercentTask;using S=T::Stage;const auto stage=percent_->stage();percent_->advanceBefore(cost,[&](T::Read q){return q==T::Read::Divisor?input_.divisor:input_.numerator;});
  if(stage==S::HoldOutput)effect_.write=true;if(stage==S::RestoreOutput){presentation_=input_.presentation;effect_.read=true;}if(stage==S::Finish)effect_.read=true;if(stage==S::SetPrefix)setByte(0x8d);if(stage==S::SetSuffix)setByte('%');
  if(percent_->child()!=T::Child::None){effect_.write=true;if(percent_->child()==T::Child::Value)startLcd(PanelSettingsLcdService::Kind::TwoZeros,percent_->value());else startLcd(PanelSettingsLcdService::Kind::Character);}
  if(percent_->stage()==S::Complete){summary_.percentDivisor=percent_->divisor();summary_.percentResult=percent_->packedResult();complete_=true;}break;}
 }
 summary_.presentation=presentation_;return cost;}
private:
 void setByte(uint8_t v){presentation_=uint16_t((presentation_&0xff00)|v);}void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void startVersion(PanelVersionTask::Kind k){if(depth_==versions_.size())throw std::logic_error("Panel version depth exceeded");versions_[depth_++].emplace(k);select(Component::Version);}
 void startLcd(PanelSettingsLcdService::Kind k,uint16_t value=0,uint32_t source=0){lcdOwner_=component_;lcd_.emplace(k,PanelSettingsLcdService::Input{value,presentation_,source});select(Component::Lcd);}
 Kind kind_;Input input_;Summary summary_{};Effect effect_{};Component component_=Component::Clock,lcdOwner_=Component::Clock;uint32_t components_=0;uint16_t presentation_=0;bool complete_=false,barTail_=false;unsigned depth_=0;
 std::array<std::optional<PanelVersionTask>,2>versions_;std::optional<ClockDisplayTask>clock_;std::optional<SystemServicesTask>bar_;std::optional<PanelPercentTask>percent_;std::optional<PanelSettingsLcdService>lcd_;
};
}
