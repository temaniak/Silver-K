#pragma once
#include "ksStateMenuActionsTask.h"
#include "ksStateStoreUiTask.h"
#include "ksStateStoreService.h"
#include <optional>
#include <stdexcept>
namespace ks {
// Store policy and page transitions. Outgoing operations are semantic native
// children; their ownership and completed observations belong to the host.
class StateStoreMenuCoordinator final {
public:
 enum class Kind{Menu,BeginProgram,BeginName,BeginStore};
 enum class Component{Menu,Page};
 enum class Request{None,Gate,Coefficients,GlobalSave,Notice,ClearEdit,Release,Dump,Factory,Check,Resolve,Name,Playback,CommitMulti,CommitSingle};
 struct Input{uint16_t event=0;bool pressed=true;};
 struct Effect{bool read=false,write=false;};
 struct Summary{uint16_t presentation=0;uint32_t noticeSource=0;uint8_t edit=0,screen=0,page=0,ui=0,mode=0,part=0;bool gate=false,calibration=false,global=false,checked=false,allowed=false,name=false,resolve=false,playback=false,commitMulti=false,commitSingle=false,dump=false,factory=false,release=false,clear=false,pageStarted=false;Kind pageKind=Kind::BeginStore;};
 StateStoreMenuCoordinator(Kind k):StateStoreMenuCoordinator(k,Input{}){}
 StateStoreMenuCoordinator(Kind k,Input i):kind_(k),input_(i){summary_.presentation=i.event;if(k==Kind::Menu)menu_.emplace(StateMenuActionsTask::Kind::Store,i.event,i.pressed);else startPage(k);}
 Kind kind()const{return kind_;}const Input&input()const{return input_;}Component component()const{return menu_&&!page_?Component::Menu:Component::Page;}const Summary&summary()const{return summary_;}bool complete()const{return complete_;}Request request()const{return request_;}const Effect&effect()const{return effect_;}
 const StateMenuActionsTask*menuTask()const{return menu_?&*menu_:nullptr;}const StateStoreUiTask*pageTask()const{return page_?&*page_:nullptr;}
 bool resume(uint16_t presentation=0,bool accepted=false){if(request_==Request::None)return false;const auto old=request_;request_=Request::None;summary_.presentation=presentation;
  if(tail_){complete_=true;return true;}if(old==Request::Gate){summary_.gate=true;menu_->resumeChild(accepted);}else if(old==Request::Coefficients){summary_.calibration=true;menu_->resumeChild();}else if(old==Request::GlobalSave){summary_.global=true;menu_->resumeChild();}else if(old==Request::Notice)menu_->resumeChild();else if(old==Request::Check){summary_.checked=true;summary_.allowed=accepted;page_->resumeChild(uint8_t(accepted));}else if(old==Request::Resolve){summary_.resolve=true;page_->resumeChild();}else if(old==Request::Name){summary_.name=true;page_->resumeChild();}else throw std::logic_error("Store menu child has no native continuation");return true;}
 unsigned nextClocks(int previous=-1)const{if(complete_||request_!=Request::None)return 0;if(page_)return page_->nextClocks(byteClock(pageTarget(),previous));return menu_->nextClocks(byteClock(menuTarget(),previous));}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};const auto byte=[&](uint32_t a){return uint8_t(read(a,cost));};
  if(!page_){using T=StateMenuActionsTask;using S=T::Stage;const auto s=menu_->stage();menu_->advanceBefore(cost,[&](T::Read q)->uint32_t{switch(q){case T::Read::Edit:return summary_.edit=byte(0x16784);case T::Read::Screen:return summary_.screen=byte(0x16785);case T::Read::Page:return summary_.page=byte(0x16787);case T::Read::UiPage:return summary_.ui=byte(0x16790);case T::Read::Mode:return summary_.mode=byte(0x167ba);case T::Read::Part:return summary_.part=byte(0x16799);default:throw std::logic_error("Unexpected store menu decision read");}},[&](T::Write q,uint8_t v){write(q==T::Write::Special?0x1679e:0x1679f,v,cost);},byteClock(menuTarget(),previous));
   if(menu_->awaitingChild()){effect_.write=true;switch(s){case S::ObserveCalibrationGate:ask(Request::Gate);break;case S::SaveCalibration:ask(Request::Coefficients);break;case S::StoreProgram:ask(Request::GlobalSave);break;case S::ShowCalibrationNotice:case S::ShowProgramNotice:ask(Request::Notice);break;default:throw std::logic_error("Unexpected store menu child");}}
   if(s==S::BindCalibrationNotice)summary_.noticeSource=0x14ec9;if(s==S::BindProgramNotice)summary_.noticeSource=0x14eba;
   if(menu_->complete()){using E=T::Exit;switch(menu_->exit()){case E::None:effect_.read=true;complete_=true;break;case E::Release:summary_.release=true;ask(Request::Release,true);break;case E::Notice:summary_.clear=true;ask(Request::ClearEdit,true);break;case E::FirstPage:summary_.dump=true;ask(Request::Dump,true);break;case E::SecondPage:summary_.factory=true;ask(Request::Factory,true);break;case E::ProgramStore:startPage(Kind::BeginProgram);break;case E::MultiStore:startPage(Kind::BeginName);break;case E::PartStore:startPage(Kind::BeginStore);break;case E::CommonStore:summary_.commitMulti=true;ask(Request::CommitMulti,true);break;case E::PanelStore:summary_.commitSingle=true;ask(Request::CommitSingle,true);break;default:throw std::logic_error("Unexpected store menu tail");}}
  }else {using T=StateStoreUiTask;using S=T::Stage;const auto s=page_->stage();page_->advanceBefore(cost,[](T::Read)->uint32_t{throw std::logic_error("Unexpected store page read");},[&](T::Write q,uint8_t v){write(q==T::Write::Dirty?0x16799:0x16790,v,cost);},byteClock(pageTarget(),previous));
   if(page_->awaitingChild()){effect_.write=true;switch(page_->child()){case T::Child::CheckProtection:ask(Request::Check);break;case T::Child::RefreshName:ask(Request::Resolve);break;case T::Child::NameEditor:ask(Request::Name);break;default:throw std::logic_error("Unexpected store page child");}}
   if(page_->awaitingTail()){if(page_->tail()!=T::Tail::FinishPage)throw std::logic_error("Unexpected store page tail");summary_.playback=true;ask(Request::Playback,true);}else if(page_->complete()){effect_.read=true;complete_=true;}
  }return cost;
 }
private:
 static unsigned byteClock(uint32_t a,int p){return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0u:3u;}
 uint32_t menuTarget()const{return menu_&&menu_->stage()==StateMenuActionsTask::Stage::ClearStoreSpecial?0x1679e:0x1679f;}
 uint32_t pageTarget()const{using S=StateStoreUiTask::Stage;return page_&&(page_->stage()==S::WriteProgramDirty)?0x16799:0x16790;}
 void startPage(Kind k){summary_.pageStarted=true;summary_.pageKind=k;page_.emplace(k==Kind::BeginProgram?StateStoreUiTask::Kind::BeginProgram:k==Kind::BeginName?StateStoreUiTask::Kind::BeginName:StateStoreUiTask::Kind::BeginStore);}
 void ask(Request r,bool tail=false){if(request_!=Request::None)throw std::logic_error("Store menu request ownership");request_=r;tail_=tail;}
 Kind kind_;Input input_;Summary summary_{};std::optional<StateMenuActionsTask>menu_;std::optional<StateStoreUiTask>page_;Request request_=Request::None;Effect effect_{};bool tail_=false,complete_=false;
};}
