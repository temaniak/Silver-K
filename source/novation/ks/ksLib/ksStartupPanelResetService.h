#pragma once
#include "ksPanelInitializeTask.h"
#include "ksPanelControlTask.h"
#include "ksStartupLcdActionService.h"
#include "ksStartupHardwareService.h"
#include <optional>
namespace ks {
class StartupPanelControlService final {
public:
 struct Effect{bool read=false,write=false;};
 explicit StartupPanelControlService(uint8_t selector,uint8_t value=0):selector_(selector),value_(value){if(selector_>=20)throw std::logic_error("Startup control selector range");}
 bool complete()const{return task_.stage()==PanelControlTask::Stage::Complete;}unsigned priority()const{return 0;}const Effect&effect()const{return effect_;}
 unsigned nextClocks(int previous=-1)const{return task_.nextClocks([&](PanelControlTask::Write q){return StartupHardwareService::byteClocks(target(q),previous);});}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;using T=PanelControlTask;using S=T::Stage;const auto stage=task_.stage();effect_={true,stage==S::RetainFirst||stage==S::RetainSecond||stage==S::RetainThird||stage==S::RetainFourth||stage==S::RetainInput||stage==S::HoldDelay};
  return task_.advanceBefore(cost,[&](T::Read q)->uint32_t{const auto table=0x1c0u+task_.tableOffset();switch(q){case T::Read::Selector:return selector_;case T::Read::Input:return value_;case T::Read::Offset:return read(table,cost);case T::Read::Limit:return read(table+3,cost);case T::Read::Mode:return read(table+4,cost);case T::Read::Mask:return read(table+2,cost);case T::Read::Set:return read(table+1,cost);case T::Read::Mirror:return read(0xffec1c+task_.offset(),cost);case T::Read::Port:return read(0xffff62,cost-1);}throw std::logic_error("Startup control observation");},[&](T::Write q,uint8_t value){write(target(q),value,cost);},[&](T::Write q){return StartupHardwareService::byteClocks(target(q),previous);});
 }
private:
 uint32_t target(PanelControlTask::Write q)const{return q==PanelControlTask::Write::Mirror?0xffec1cu+task_.offset():q==PanelControlTask::Write::Data?0xffff61u:0xffff62u;}
 PanelControlTask task_;uint8_t selector_=0,value_=0;Effect effect_{};
};
class StartupPanelResetService final {
public:
 using Kind=PanelInitializeTask::Kind;struct Effect{bool read=false,write=false;};
 explicit StartupPanelResetService(Kind kind=Kind::Reset):policy_(kind){}
 bool complete()const{return policy_.stage()==PanelInitializeTask::Stage::Complete;}unsigned priority()const{return 0;}const Effect&effect()const{return effect_;}
 unsigned nextClocks(int previous=-1)const{return control_?control_->nextClocks(previous):lcd_?lcd_->nextClocks(previous):policy_.nextClocks();}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};
  if(control_){control_->advanceBefore(cost,read,write,previous);effect_={control_->effect().read,control_->effect().write};if(control_->complete()){control_.reset();if(!policy_.resumeChild())throw std::logic_error("Startup panel control return");}return cost;}
  if(lcd_){lcd_->advanceBefore(cost,read,write,previous);effect_={lcd_->effect().read,lcd_->effect().write};if(lcd_->complete()){lcd_.reset();if(!policy_.resumeChild())throw std::logic_error("Startup panel wake return");}return cost;}
  using T=PanelInitializeTask;using S=T::Stage;const auto stage=policy_.stage();effect_={true,stage==S::Hold};policy_.advanceBefore(cost,[&](T::Write q,uint8_t value){const auto a=q==T::Write::Holdoff?0xffec23u:q==T::Write::DecayMask?0xffec16u:q==T::Write::DecayInvert?0xffec24u:q==T::Write::PendingMask?0xffec1bu:q==T::Write::PendingInvert?0xffec29u:q==T::Write::Invert?0xffec28u:0xffec1au;const unsigned width=q==T::Write::DecayMask||q==T::Write::DecayInvert?4:1;for(unsigned n=0;n<width;++n)write(a+n,value,cost-3*(width-n-1));});
  if(policy_.child()==T::Child::Wake){lcd_.emplace(StartupLcdActionService::Kind::Command,0x0e);effect_.write=true;}else if(policy_.child()==T::Child::Control){control_.emplace(uint8_t(policy_.index()),0);effect_.write=true;}return cost;
 }
private:PanelInitializeTask policy_;std::optional<StartupPanelControlService>control_;std::optional<StartupLcdActionService>lcd_;Effect effect_{};
};
}
