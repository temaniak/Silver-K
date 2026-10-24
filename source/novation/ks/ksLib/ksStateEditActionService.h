#pragma once
#include "ksStateScreenActionsTask.h"
#include "ksStateProgramBrowseTask.h"
#include "ksStateFieldService.h"
#include "ksPanelSettingsService.h"
#include "ksStateScreenService.h"
#include "ksStateApplyService.h"
#include <optional>
#include <stdexcept>
namespace ks {
// A complete edit-button transaction. Every child is a typed native service;
// retained observations describe the UI operation, never an execution frame.
class StateEditActionService final {
public:
 enum class Kind {ToggleEdit,SelectEdit,ToggleOne,ToggleTwo,ClearDigits,ClearEdit,Release};
 enum class Component {Action,ClearDigits,Gate,CalibrationReset,ScreenSet,Apply,UiRefresh};
 struct Input {uint16_t event=0;bool pressed=true;};
 struct Effect {bool read=false,write=false;};
 struct Summary {uint16_t presentation=0;uint32_t patch=0,reference=0;uint8_t page=0,editBefore=0,editAfter=0,screen=0,valueBefore=0,resetLast=0;bool pageObserved=false,editObserved=false,screenObserved=false,patchObserved=false,clearDigits=false,gate=false,calibration=false,screenSet=false,apply=false,ui=false,released=false;std::optional<StateScreenService::Summary>gateOutcome,setOutcome;};
 explicit StateEditActionService(Kind k):StateEditActionService(k,Input{}){}
 StateEditActionService(Kind k,Input in):kind_(k),input_(in),action_(actionKind(k),in.event,in.pressed){summary_.presentation=in.event;if(k==Kind::ClearDigits){digits_.emplace(StateProgramBrowseTask::Kind::Clear);select(Component::ClearDigits);}else select(Component::Action);}
 Kind kind()const{return kind_;}const Input&input()const{return input_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return complete_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 const StateScreenActionsTask&actionTask()const{return action_;}const StateProgramBrowseTask*digitsTask()const{return digits_?&*digits_:nullptr;}const StateFieldService*ui()const{return ui_?&*ui_:nullptr;}const StateApplyService*apply()const{return apply_?&*apply_:nullptr;}
 bool critical()const{if(component_==Component::Apply)return apply_->critical();if(component_==Component::ScreenSet)return screen_->critical();if(component_==Component::UiRefresh)return ui_->critical();return false;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,StateScreenActionsTask>)return component_==Component::Action?&action_:nullptr;else if constexpr(std::is_same_v<T,StateProgramBrowseTask>)return component_==Component::ClearDigits&&digits_?&*digits_:nullptr;else if constexpr(std::is_same_v<T,StateScreenService>)return screen_?&*screen_:nullptr;else if constexpr(std::is_same_v<T,PanelSettingsService>)return settings_?&*settings_:nullptr;else if constexpr(std::is_same_v<T,StateApplyService>)return component_==Component::Apply&&apply_?&*apply_:nullptr;else if(component_==Component::UiRefresh&&ui_)return ui_->template task<T>();else if(screen_)return screen_->template task<T>();else return nullptr;}
 static bool dataSpan(uint32_t a,unsigned n){return StateStoreService::dataSpan(a,n);}
 template<class Peek>static bool admitted(Kind k,const Input&in,Peek&&peek){
  if(k==Kind::ClearDigits||k==Kind::ClearEdit||k==Kind::Release)return true;
  if((k!=Kind::SelectEdit&&!in.pressed)||(k==Kind::SelectEdit&&!uint8_t(in.event>>8)))return true;
  const auto wide=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(peek((a+n)&0xffffff));return v;};
  if((k==Kind::ToggleEdit||k==Kind::SelectEdit)&&uint8_t(peek(0x16790)))return true;
  if(!dataSpan(wide(0x167bc),256)||!dataSpan(wide(0x167c0),256))return false;
  if(k==Kind::SelectEdit){StateScreenService::Input s;s.index=uint8_t(in.event);s.character=in.event;if(!StateScreenService::admitted(StateScreenService::Kind::Set,s,peek))return false;}
  if(k==Kind::ToggleOne||k==Kind::ToggleTwo){const auto over=wide(0x167a6);if(over&&!dataSpan(over,256))return false;if(uint8_t(peek(0xffed6c))||uint8_t(peek(0xffed6e)))return false;}
  return true;
 }
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;switch(component_){case Component::Action:return action_.nextClocks(actionWriteClock(action_.stage(),previous));case Component::ClearDigits:return digits_->nextClocks(byteClock(0x167fb,previous));case Component::Gate:case Component::ScreenSet:return screen_->nextClocks(previous);case Component::CalibrationReset:return settings_->nextClocks(previous);case Component::Apply:return apply_->nextClocks(previous);case Component::UiRefresh:return ui_->nextClocks(previous);}return 0;}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolver,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  const auto byte=[&](uint32_t raw,unsigned back=0){if(!dataSpan(raw,1))throw std::logic_error("Unsupported evolving edit action observation");return uint8_t(read(raw&0xffffff,cost-back));};
  const auto put=[&](uint32_t a,uint8_t v){write(a,v,cost);};
  if(component_==Component::Action){using T=StateScreenActionsTask;using S=T::Stage;const auto stage=action_.stage();
   action_.advanceBefore(cost,[&](T::Read q,uint32_t at)->uint32_t{switch(q){case T::Read::UiPage:summary_.pageObserved=true;return summary_.page=byte(0x16790);case T::Read::Edit:summary_.editObserved=true;return summary_.editBefore=byte(0x16784);case T::Read::Screen:summary_.screenObserved=true;return summary_.screen=byte(0x16785);case T::Read::PatchBase:{uint32_t base=0;for(unsigned n=0;n<4;++n)base=(base<<8)|byte(0x167bc+n,3*((3-n)/2));if(!dataSpan(base,256))throw std::logic_error("Unsupported evolving edit action patch base");summary_.patchObserved=true;summary_.patch=base;return base;}case T::Read::PatchByte:return summary_.valueBefore=byte(at);default:throw std::logic_error("Unexpected edit action read");}},[&](T::Write q,uint16_t v){if(q==T::Write::Latch)put(0x16792,uint8_t(v));else if(q==T::Write::Edit){summary_.editAfter=uint8_t(v);put(0x16784,uint8_t(v));}else throw std::logic_error("Unexpected edit action publication");},actionWriteClock(stage,previous));
   if(stage==S::ReadEditEventScreen)summary_.presentation=uint16_t((unsigned(summary_.screen)<<8)|uint8_t(summary_.presentation));
   if(stage==S::ToggleEditValue)summary_.presentation=uint16_t((unsigned(action_.edit())<<8)|uint8_t(summary_.presentation));
   if(stage==S::FlipPatchToggleOne||stage==S::FlipPatchToggleTwo)summary_.presentation=uint16_t((unsigned(action_.value())<<8)|uint8_t(summary_.presentation));
   if(action_.awaitingChild()){effect_.write=true;switch(stage){case S::ResetEditToggleContext:case S::ResetEditEventContext:digits_.emplace(StateProgramBrowseTask::Kind::Clear);select(Component::ClearDigits);break;case S::ObserveEditToggleGate:startScreen(StateScreenService::Kind::Gate,Component::Gate);break;case S::ReloadEditCalibration:settings_.emplace(PanelSettingsService::Kind::Reset);select(Component::CalibrationReset);break;case S::ApplyEditEvent:startScreen(StateScreenService::Kind::Set,Component::ScreenSet);break;default:throw std::logic_error("Unexpected edit action child");}}
   else if(action_.complete()){switch(action_.exit()){case T::Exit::None:effect_.read=true;complete_=true;break;case T::Exit::Release:summary_.released=true;action_=T(T::Kind::Return);select(Component::Action);break;case T::Exit::Refresh:startUi();break;case T::Exit::ApplyPatch:summary_.reference=summary_.patch+(kind_==Kind::ToggleOne?215u:213u);apply_.emplace(StateApplyService::Kind::Apply,StateApplyService::Input{summary_.reference,summary_.presentation});select(Component::Apply);break;default:throw std::logic_error("Unexpected edit action tail");}}
  }else if(component_==Component::ClearDigits){using T=StateProgramBrowseTask;const auto stage=digits_->stage();digits_->advanceBefore(cost,[](auto,auto)->uint32_t{throw std::logic_error("Unexpected clear-digit read");},[&](T::Write q,uint8_t v){if(q!=T::Write::Latch)throw std::logic_error("Unexpected clear-digit publication");put(0x167fb,v);},byteClock(0x167fb,previous));if(stage==T::Stage::SaveClearDigits)effect_.write=true;if(stage==T::Stage::RestoreClearDigits||stage==T::Stage::ReturnClearDigits)effect_.read=true;if(digits_->complete()){summary_.clearDigits=true;digits_.reset();if(kind_==Kind::ClearDigits)complete_=true;else resume(false);}
  }else if(component_==Component::Gate||component_==Component::ScreenSet){const bool gate=component_==Component::Gate;screen_->advanceBefore(cost,read,write,resolver,previous);effect_={screen_->effect().read,screen_->effect().write};if(screen_->complete()){if(gate){summary_.gate=true;summary_.gateOutcome=screen_->summary();const bool accepted=screen_->summary().gate.accepted;screen_.reset();resume(accepted);}else{summary_.screenSet=true;summary_.setOutcome=screen_->summary();summary_.presentation=screen_->presentation();screen_.reset();resume(false);}}
  }else if(component_==Component::CalibrationReset){settings_->advanceBefore(cost,read,write,resolver,previous);effect_={settings_->effect().read,settings_->effect().write};if(settings_->complete()){summary_.calibration=true;summary_.resetLast=settings_->copiedLastByte();settings_.reset();resume(false);}
  }else if(component_==Component::Apply){const bool retained=apply_->clearsWriteHistory(),released=apply_->clearsReadHistory();apply_->advanceBefore(cost,read,write,resolver,previous);effect_={released,retained};if(apply_->complete()){summary_.apply=true;complete_=true;}
  }else {ui_->advanceBefore(cost,read,write,resolver,previous);effect_={ui_->effect().read,ui_->effect().write};if(ui_->complete()){summary_.ui=true;complete_=true;}}
  return cost;
 }
private:
 static StateScreenActionsTask::Kind actionKind(Kind k){using T=StateScreenActionsTask;return k==Kind::ToggleEdit?T::Kind::ToggleEdit:k==Kind::SelectEdit?T::Kind::SelectEdit:k==Kind::ToggleOne?T::Kind::ToggleOne:k==Kind::ToggleTwo?T::Kind::ToggleTwo:k==Kind::ClearEdit?T::Kind::ClearEdit:T::Kind::Return;}
 static unsigned byteClock(uint32_t a,int p){a&=0xffffff;return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 static unsigned actionWriteClock(StateScreenActionsTask::Stage s,int p){using S=StateScreenActionsTask::Stage;return byteClock(s==S::WriteEditToggleLatch||s==S::WriteEditEventLatch||s==S::WriteEditLatch?0x16792:0x16784,p);}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}void resume(bool nonzero){if(!action_.resumeChild(nonzero))throw std::logic_error("Invalid edit action native child completion");select(Component::Action);}
 void startScreen(StateScreenService::Kind k,Component c){StateScreenService::Input in;in.index=uint8_t(summary_.presentation);in.character=summary_.presentation;screen_.emplace(k,in);select(c);}
 void startUi(){ui_.emplace(StateUiRefreshService::Kind::Refresh,StateUiRefreshService::Input{0,summary_.presentation,summary_.presentation});select(Component::UiRefresh);}
 Kind kind_;Input input_;StateScreenActionsTask action_;Component component_=Component::Action;uint32_t components_=0;bool complete_=false;Effect effect_{};Summary summary_{};std::optional<StateProgramBrowseTask>digits_;std::optional<StateScreenService>screen_;std::optional<PanelSettingsService>settings_;std::optional<StateApplyService>apply_;std::optional<StateFieldService>ui_;
};}
