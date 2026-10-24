#pragma once
#include "ksStateScreenActionsTask.h"
#include "ksStateBankNavigationTask.h"
#include "ksStatePlaybackModeCoordinator.h"
#include "ksStateFieldService.h"
#include <optional>
#include <stdexcept>
namespace ks {
// Panel step decisions and part selection, including every typed routing,
// name/category/scene edit and refresh child. No execution frame is retained.
class StatePartNavigationService final {
public:
 enum class Kind {Down,Up,Step,PartStep,Return};
 enum class Component {Action,Parts,Resolve,Routing,UiRefresh,SceneEdit};
 struct Input {uint16_t event=0;bool pressed=true;};
 struct Effect {bool read=false,write=false;};
 struct Summary {uint16_t presentation=0;uint8_t page=0,special=0,mode=0,multi=0,selected=0,marker=0,enabled=0;bool pageObserved=false,specialObserved=false,modeObserved=false,multiObserved=false,selectedObserved=false,resolved=false,routed=false,ui=false,sceneEdit=false,release=false,callEdit=false;};
 explicit StatePartNavigationService(Kind k):StatePartNavigationService(k,Input{}){}
 StatePartNavigationService(Kind k,Input in):kind_(k),input_(in),action_(k==Kind::Down?Action::Kind::Down:k==Kind::Up?Action::Kind::Up:k==Kind::Step?Action::Kind::Step:Action::Kind::Return,in.event,in.pressed){summary_.presentation=in.event;if(k==Kind::PartStep){parts_.emplace(Bank::Kind::Step,in.event);select(Component::Parts);}else select(Component::Action);}
 Kind kind()const{return kind_;}const Input&input()const{return input_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return complete_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 const StateFieldService*field()const{return field_?&*field_:nullptr;}const StatePlaybackModeCoordinator*routing()const{return routing_?&*routing_:nullptr;}
 const StateScreenActionsTask&actionTask()const{return action_;}const StateBankNavigationTask*partsTask()const{return parts_?&*parts_:nullptr;}const StateBankNavigationTask*resolveTask()const{return resolve_?&*resolve_:nullptr;}
 bool critical()const{return component_==Component::Routing?routing_->critical():(component_==Component::UiRefresh||component_==Component::SceneEdit)?field_->critical():false;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,Action>)return component_==Component::Action?&action_:nullptr;else if constexpr(std::is_same_v<T,Bank>)return component_==Component::Parts?&*parts_:component_==Component::Resolve?&*resolve_:nullptr;else if constexpr(std::is_same_v<T,StatePlaybackModeCoordinator>)return component_==Component::Routing?&*routing_:nullptr;else if constexpr(std::is_same_v<T,StateModeTask>||std::is_same_v<T,StateStorageTask>||std::is_same_v<T,StatePartSelectionTask>)return component_==Component::Routing?routing_->template task<T>():nullptr;else if((component_==Component::UiRefresh||component_==Component::SceneEdit)&&field_)return field_->template task<T>();else return nullptr;}
 static bool dataSpan(uint32_t a,unsigned n){return StateStoreService::dataSpan(a,n);}
 template<class Peek>static bool admitted(Kind k,const Input&in,Peek&&peek){
  if(k==Kind::Return||((k==Kind::Down||k==Kind::Up)&&!in.pressed))return true;
  const auto wide=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(peek(a+n));return v;};
  return dataSpan(wide(0x167bc),256)&&dataSpan(wide(0x167c0),256);
 }
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;switch(component_){case Component::Action:return action_.nextClocks();case Component::Parts:return parts_->nextClocks(partBus(parts_->stage(),previous));case Component::Resolve:return resolve_->nextClocks(byteClock(0x167d6,previous));case Component::Routing:return routing_->nextClocks(previous);case Component::UiRefresh:case Component::SceneEdit:return field_->nextClocks(previous);}return 0;}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolver,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  const auto byte=[&](uint32_t a){if(!dataSpan(a,1))throw std::logic_error("Unsupported evolving part navigation resource");return uint8_t(read(a&0xffffff,cost));};
  const auto put=[&](uint32_t a,uint8_t v){write(a,v,cost);};
  if(component_==Component::Action){using S=Action::Stage;const auto stage=action_.stage();action_.advanceBefore(cost,[&](Action::Read q,uint32_t)->uint32_t{switch(q){case Action::Read::UiPage:summary_.pageObserved=true;return summary_.page=byte(0x16790);case Action::Read::Special:summary_.specialObserved=true;return summary_.special=byte(0x1679e);case Action::Read::Mode:summary_.modeObserved=true;return summary_.mode=byte(0x167ba);case Action::Read::Multi:summary_.multiObserved=true;return summary_.multi=byte(0x167d5);default:throw std::logic_error("Unexpected part navigation decision observation");}},[](auto,auto){throw std::logic_error("Unexpected part navigation decision publication");});
   if(stage==S::BindStepDown)summary_.presentation=uint16_t((summary_.presentation&0xff00)|255);if(stage==S::BindStepUp)summary_.presentation=uint16_t((summary_.presentation&0xff00)|1);
   if(action_.awaitingChild()){if(stage!=S::ApplyProgramStep)throw std::logic_error("Unexpected part navigation action child");effect_.write=true;summary_.callEdit=true;startEdit(StateSceneEditCoordinator::Kind::Category);}
   else if(action_.complete()){switch(action_.exit()){case Action::Exit::None:effect_.read=true;complete_=true;break;case Action::Exit::Release:summary_.release=true;action_=Action(Action::Kind::Return);select(Component::Action);break;case Action::Exit::NameStep:startEdit(StateSceneEditCoordinator::Kind::MoveName);break;case Action::Exit::PanelStep:startEdit(StateSceneEditCoordinator::Kind::Category);break;case Action::Exit::SpecialStep:startEdit(StateSceneEditCoordinator::Kind::Sub);break;case Action::Exit::MultiStep:parts_.emplace(Bank::Kind::Step,summary_.presentation);select(Component::Parts);break;default:throw std::logic_error("Unexpected part navigation action tail");}}
  }else if(component_==Component::Parts){using S=Bank::Stage;const auto stage=parts_->stage();parts_->advanceBefore(cost,[&](Bank::Read q,uint32_t a)->uint32_t{if(q==Bank::Read::Part){summary_.selectedObserved=true;return summary_.selected=byte(0x16799);}if(q==Bank::Read::Enabled)return summary_.enabled=byte(a);throw std::logic_error("Unexpected part selection observation");},[&](Bank::Write q,uint32_t,uint8_t v){if(q==Bank::Write::Part)put(0x16799,v);else if(q==Bank::Write::CurrentPart)put(0x167b8,v);else if(q==Bank::Write::RequestedPart)put(0x167d0,v);else throw std::logic_error("Unexpected part selection publication");},partBus(stage,previous));
   if(parts_->awaitingChild()){effect_.write=true;if(stage==S::ResolveSelectedPart){resolve_.emplace(Bank::Kind::Resolve);select(Component::Resolve);}else if(stage==S::ApplyPartRouting){routing_.emplace(StatePlaybackModeCoordinator::Kind::Routing);select(Component::Routing);}else throw std::logic_error("Unexpected part selection child");}
   else if(stage==S::RefreshPart)startUi();
   else if(parts_->complete()){effect_.read=true;complete_=true;}
  }else if(component_==Component::Resolve){using S=Bank::Stage;const auto stage=resolve_->stage();resolve_->advanceBefore(cost,[&](Bank::Read q,uint32_t a)->uint32_t{if(q==Bank::Read::Part){summary_.selectedObserved=true;return summary_.selected=byte(0x16799);}if(q==Bank::Read::Enabled)return summary_.marker=byte(a);throw std::logic_error("Unexpected edit-part resolution observation");},[&](Bank::Write q,uint32_t,uint8_t v){if(q!=Bank::Write::Marker)throw std::logic_error("Unexpected edit-part resolution publication");put(0x167d6,v);},byteClock(0x167d6,previous));if(stage==S::ReturnPartMarker)effect_.read=true;if(resolve_->complete()){summary_.resolved=true;parts_->resumeChild(uint8_t(summary_.selected<<1));select(Component::Parts);}
  }else if(component_==Component::Routing){routing_->advanceBefore(cost,read,write,resolver,previous);effect_={routing_->effect().read,routing_->effect().write};if(routing_->complete()){summary_.routed=true;parts_->resumeChild(0);select(Component::Parts);}
  }else {const bool edit=component_==Component::SceneEdit;field_->advanceBefore(cost,read,write,resolver,previous);effect_={field_->effect().read,field_->effect().write};if(field_->complete()){if(edit){summary_.sceneEdit=true;if(summary_.callEdit){action_.resumeChild(false);select(Component::Action);}else complete_=true;}else{summary_.ui=true;complete_=true;}}}
  return cost;
 }
private:
 using Action=StateScreenActionsTask;using Bank=StateBankNavigationTask;
 static unsigned byteClock(uint32_t a,int p){a&=0xffffff;return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 static unsigned partBus(Bank::Stage s,int p){using S=Bank::Stage;return byteClock(s==S::WriteSelectedPart?0x16799:s==S::WriteCurrentPart?0x167b8:s==S::WriteRequestedPart?0x167d0:0,p);}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void startEdit(StateSceneEditCoordinator::Kind k){field_.emplace(k,StateSceneEditCoordinator::Input{summary_.presentation});select(Component::SceneEdit);}
 void startUi(){field_.emplace(StateUiRefreshService::Kind::Refresh,StateUiRefreshService::Input{0,summary_.presentation,summary_.presentation});select(Component::UiRefresh);}
 Kind kind_;Input input_;Action action_;std::optional<Bank>parts_,resolve_;std::optional<StatePlaybackModeCoordinator>routing_;std::optional<StateFieldService>field_;Component component_=Component::Action;uint32_t components_=0;bool complete_=false;Effect effect_{};Summary summary_{};
};}
