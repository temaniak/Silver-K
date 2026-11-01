#pragma once
#include "ksStateMenuActionsTask.h"
#include "ksStateEditContextTask.h"
#include "ksStateEditAddressTask.h"
#include "ksTimerServiceTask.h"
#include <optional>
namespace ks {
// Complete menu navigation, repeat publication, edit-context binding and edit
// address resolution. Outgoing actions are semantic tails, never child calls.
class StateMenuService final {
public:
 enum class Kind {Toggle,Auxiliary,Previous,RepeatPrevious,Next,RepeatNext,EditActive,RepeatArm,Clock,BindContext,ClearContext,IndexedAddress,DirectAddress,GlobalAddress};
 enum class Component {Menu,EditGate,RepeatTimer,Clock,Context,Address};
 enum class Exit {Return,Release,RefreshToggle,Auxiliary,ReleaseNavigation,PreviousName,PreviousSpecial,NextName,NextSpecial,Edit,PreviousProgram,NextProgram};
 enum class RepeatAction {Previous,Next};
 struct Input {uint16_t event=0,interval=0;bool pressed=true;uint32_t index=0,descriptor=0;RepeatAction action=RepeatAction::Previous;};
 explicit StateMenuService(Kind kind):StateMenuService(kind,Input{}){}
 StateMenuService(Kind kind,Input input):kind_(kind),interval_(input.interval),action_(input.action){
  using M=StateMenuActionsTask;using E=StateEditContextTask;using A=StateEditAddressTask;
  switch(kind){
  case Kind::Toggle:menu_.emplace(M::Kind::Toggle,input.event,input.pressed);select(Component::Menu);break;
  case Kind::Auxiliary:menu_.emplace(M::Kind::Auxiliary,input.event,input.pressed);select(Component::Menu);break;
  case Kind::Previous:menu_.emplace(M::Kind::Previous,input.event,input.pressed);select(Component::Menu);break;
  case Kind::RepeatPrevious:menu_.emplace(M::Kind::RepeatPrevious,input.event,input.pressed);select(Component::Menu);break;
  case Kind::Next:menu_.emplace(M::Kind::Next,input.event,input.pressed);select(Component::Menu);break;
  case Kind::RepeatNext:menu_.emplace(M::Kind::RepeatNext,input.event,input.pressed);select(Component::Menu);break;
  case Kind::EditActive:select(Component::EditGate);break;
  case Kind::RepeatArm:select(Component::RepeatTimer);break;
  case Kind::Clock:clock_.emplace();select(Component::Clock);break;
  case Kind::BindContext:context_.emplace(E::Kind::Bind,input.index);select(Component::Context);break;
  case Kind::ClearContext:context_.emplace(E::Kind::Clear);select(Component::Context);break;
  case Kind::IndexedAddress:address_.emplace(A::Kind::Indexed,input.descriptor);select(Component::Address);break;
  case Kind::DirectAddress:address_.emplace(A::Kind::Direct,input.descriptor);select(Component::Address);break;
  case Kind::GlobalAddress:address_.emplace(A::Kind::Global,input.descriptor);select(Component::Address);break;
  }
 }
 Kind kind()const{return kind_;}Component component()const{return component_;}uint32_t components()const{return components_;}
 bool complete()const{return complete_;}Exit exit()const{return exit_;}RepeatAction repeatAction()const{return action_;}
 uint8_t edit()const{return edit_;}uint16_t interval()const{return interval_;}uint32_t observedClock()const{return observedClock_;}
 unsigned timerPhase()const{return timerPhase_;}unsigned gatePhase()const{return gatePhase_;}
 const StateMenuActionsTask* menuTask()const{return menu_?&*menu_:nullptr;}
 const StateEditContextTask* contextTask()const{return context_?&*context_:nullptr;}
 const StateEditAddressTask* addressTask()const{return address_?&*address_:nullptr;}
 const ControllerClockReadTask* clockTask()const{return clock_?&*clock_:nullptr;}
 unsigned nextClocks(int previousWrite=-1)const{
  if(complete_)return 0;
  switch(component_){case Component::Menu:return menu_->nextClocks(byteClocks(0x167a1,previousWrite));
  case Component::EditGate:return gatePhase_?17:12;
  case Component::RepeatTimer:return timerPhase_==0?18:timerPhase_==1?12:timerPhase_==2?20:timerPhase_==3?12:17;
  case Component::Clock:return clock_->nextClocks();case Component::Context:return context_->nextClocks();
  case Component::Address:return address_->nextClocks();}return 0;
 }
 // Repeat publication is an encoded action resource: its code-address bytes
 // are supplied by the authenticated outer edge, outside the native graph.
 template<class Reader,class Writer,class ActionWriter>
 unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,ActionWriter&&writeAction,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;
  const auto longValue=[&](uint32_t at){const bool paired=at<0x400000;uint32_t value=0;for(unsigned n=0;n<4;++n)value=(value<<8)|uint8_t(read((at+n)&0xffffff,cost-(paired?3u*((3-n)/2):3u*(3-n))));return value;};
  const auto writeWord=[&](uint32_t at,uint16_t value){write(at,uint8_t(value>>8),cost);write(at+1,uint8_t(value),cost);};
  if(component_==Component::Menu){
   using T=StateMenuActionsTask;using S=T::Stage;const auto stage=menu_->stage();
   menu_->advanceBefore(cost,[&](T::Read field){return uint32_t(read(field==T::Read::Toggle?0x167a1:field==T::Read::UiPage?0x16790:0x1679e,stage==S::FlipToggle?cost-byteClocks(0x167a1,previousWrite):cost));},
    [&](T::Write,uint8_t value){write(0x167a1,value,cost);},byteClocks(0x167a1,previousWrite));
   switch(stage){case S::BindPreviousDelay:case S::BindNextDelay:interval_=400;break;
   case S::BindPreviousRepeat:case S::BindNextRepeat:interval_=200;break;
   case S::BindPreviousCallback:action_=RepeatAction::Previous;break;case S::BindNextCallback:action_=RepeatAction::Next;break;default:break;}
   if(menu_->awaitingChild()){
    if(stage==S::ObservePreviousEdit||stage==S::ObserveNextEdit){gatePhase_=0;select(Component::EditGate);}
    else {timerPhase_=0;select(Component::RepeatTimer);}
   }else if(menu_->complete()){
    switch(menu_->exit()){case T::Exit::Release:finish(Exit::Release);break;case T::Exit::RefreshToggle:finish(Exit::RefreshToggle);break;case T::Exit::Auxiliary:finish(Exit::Auxiliary);break;
    case T::Exit::ReleaseNavigation:finish(Exit::ReleaseNavigation);break;case T::Exit::PreviousName:finish(Exit::PreviousName);break;case T::Exit::PreviousSpecial:finish(Exit::PreviousSpecial);break;
    case T::Exit::NextName:finish(Exit::NextName);break;case T::Exit::NextSpecial:finish(Exit::NextSpecial);break;case T::Exit::Edit:finish(Exit::Edit);break;
    case T::Exit::PreviousProgram:finish(Exit::PreviousProgram);break;case T::Exit::NextProgram:finish(Exit::NextProgram);break;default:break;}
   }
  }else if(component_==Component::EditGate){
   if(!gatePhase_){edit_=uint8_t(read(0x16784,cost));gatePhase_=1;}
   else if(menu_){menu_->resumeChild(bool(edit_));select(Component::Menu);}else finish(Exit::Return);
  }else if(component_==Component::RepeatTimer){
   switch(timerPhase_){case 0:writeAction(action_,cost);++timerPhase_;break;
   case 1:writeWord(0x16772,interval_);++timerPhase_;break;
   case 2:++timerPhase_;clock_.emplace();select(Component::Clock);break;
   case 3:writeWord(0x16770,uint16_t(observedClock_));++timerPhase_;break;
   default:if(menu_){menu_->resumeChild();select(Component::Menu);}else finish(Exit::Return);break;}
  }else if(component_==Component::Clock){
   clock_->advanceBefore(cost,[&]{return longValue(0xffec10);});
   if(clock_->complete()){observedClock_=clock_->counter();if(kind_==Kind::Clock)finish(Exit::Return);else select(Component::RepeatTimer);}
  }else if(component_==Component::Context){
   using T=StateEditContextTask;
   context_->advanceBefore(cost,[&](T::Read field,uint32_t ref){return uint32_t(read(field==T::Read::ContextKind?(ref+71586)&0xffffff:0x16785,cost));},
    [&](T::Write,uint32_t value){for(unsigned n=0;n<4;++n)write(0x167a6+n,uint8_t(value>>(24-8*n)),cost-3*((3-n)/2));});
   if(context_->complete())finish(Exit::Return);
  }else{
   using T=StateEditAddressTask;
   address_->advanceBefore(cost,[&](T::Read field){switch(field){case T::Read::IndexBase:case T::Read::TargetBase:case T::Read::DirectBase:return longValue(0x167bc);
   case T::Read::Selector:return uint32_t(read((address_->descriptor()+2)&0xffffff,cost));
   case T::Read::Index:return uint32_t(read(address_->selectorAddress()&0xffffff,cost));
   case T::Read::MappedOffset:return uint32_t(read(address_->mappingAddress()&0xffffff,cost));
   case T::Read::DirectOffset:case T::Read::GlobalOffset:return uint32_t(read((address_->descriptor()+3)&0xffffff,cost));}return uint32_t(0);});
   if(address_->complete())finish(Exit::Return);
  }
  return cost;
 }
private:
 static unsigned byteClocks(uint32_t address,int previous){return address<0x400000&&!(previous&1)&&address==(uint32_t(previous)|1u)?0:3;}
 void select(Component value){component_=value;components_|=1u<<unsigned(value);}void finish(Exit value){exit_=value;complete_=true;}
 Kind kind_;Component component_=Component::Menu;Exit exit_=Exit::Return;uint32_t components_=0;bool complete_=false;
 uint16_t interval_=0;RepeatAction action_;uint8_t edit_=0;unsigned gatePhase_=0,timerPhase_=0;uint32_t observedClock_=0;
 std::optional<StateMenuActionsTask>menu_;std::optional<StateEditContextTask>context_;std::optional<StateEditAddressTask>address_;std::optional<ControllerClockReadTask>clock_;
};
}
