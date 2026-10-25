#pragma once
#include "ksStateStoreMenuCoordinator.h"
#include "ksStateStoreDumpService.h"
#include "ksStateBrowseWorkflowTask.h"
#include "ksStateScreenService.h"
#include "ksPanelSettingsService.h"
#include "ksStatePlaybackModeService.h"
#include "ksStateFactoryService.h"
#include <array>
#include <optional>
#include <stdexcept>
namespace ks {
// Complete Store-key transaction and all store-page entries. Every outgoing
// operation is an owned native graph, including dump, Flash and return paths.
class StateStoreMenuService final {
public:
 using Kind=StateStoreMenuCoordinator::Kind;using Input=StateStoreMenuCoordinator::Input;using Effect=StateStoreMenuCoordinator::Effect;
 enum class Component{Menu,Page,Gate,Settings,Save,Resolve,Name,Playback,Action,DumpNotice,Dump,Factory};
 struct Summary{uint16_t presentation=0;uint8_t selected=0,enabled=0;bool resolve=false;};
 explicit StateStoreMenuService(Kind k):StateStoreMenuService(k,Input{}){}
 StateStoreMenuService(Kind k,Input in):coordinator_(k,in){summary_.presentation=in.event;select(coordinator_.component()==StateStoreMenuCoordinator::Component::Menu?Component::Menu:Component::Page);}
 Kind kind()const{return coordinator_.kind();}const Input&input()const{return coordinator_.input();}const StateStoreMenuCoordinator&coordinator()const{return coordinator_;}const Summary&summary()const{return summary_;}bool complete()const{return coordinator_.complete();}Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}
 unsigned priority()const{if(component_==Component::Save)return save()->writePriority();if(component_==Component::Settings)return settings_->writePriority()?6u:0u;if(component_==Component::Playback)return playback_->critical()?7u:0u;if(component_==Component::Name)return name_->critical()?7u:0u;if(component_==Component::Dump)return dump_->critical()?7u:0u;if(component_==Component::Factory)return factory_->writePriority();return 0;}
 const StateScreenService*gate()const{return gate_?&*gate_:nullptr;}const PanelSettingsService*settings()const{return settings_?&*settings_:nullptr;}const StateSaveService*save()const{return saveIndex_<saves_.size()&&saves_[saveIndex_]?&*saves_[saveIndex_]:nullptr;}const StateSaveService*saveOutcome(unsigned n)const{return n<saves_.size()&&saves_[n]?&*saves_[n]:nullptr;}unsigned saveCount()const{return saveCount_;}const StateFieldService*name()const{return name_?&*name_:nullptr;}const StatePlaybackModeService*playback()const{return playback_?&*playback_:nullptr;}const StateStoreDumpService*dump()const{return dump_?&*dump_:nullptr;}const StateFactoryService*factory()const{return factory_?&*factory_:nullptr;}
 const StateBrowseWorkflowTask*noticeTask()const{return component_==Component::DumpNotice&&notice_?&*notice_:nullptr;}const StateBankNavigationTask*resolveTask()const{return component_==Component::Resolve&&resolve_?&*resolve_:nullptr;}const StateScreenActionsTask*actionTask()const{return component_==Component::Action&&action_?&*action_:nullptr;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,StateStoreMenuCoordinator>){if(component_==Component::Menu||component_==Component::Page)return &coordinator_;}
  if constexpr(std::is_same_v<T,StateMenuActionsTask>){if(component_==Component::Menu)return coordinator_.menuTask();}
  if constexpr(std::is_same_v<T,StateStoreUiTask>){if(component_==Component::Page)return coordinator_.pageTask();}
  if constexpr(std::is_same_v<T,StateBankNavigationTask>){if(component_==Component::Resolve)return resolveTask();}
  if constexpr(std::is_same_v<T,StateBrowseWorkflowTask>){if(component_==Component::DumpNotice)return noticeTask();}
  if constexpr(std::is_same_v<T,StateScreenActionsTask>){if(component_==Component::Action)return actionTask();}
  if constexpr(std::is_same_v<T,StateSaveService>){if(component_==Component::Save)return save();}
  if constexpr(std::is_same_v<T,StateScreenService>){if(component_==Component::Gate)return gate();}
  if constexpr(std::is_same_v<T,PanelSettingsService>){if(component_==Component::Settings)return settings();}
  if constexpr(std::is_same_v<T,StateScreenTask>){if(component_==Component::Gate)return gate()->template task<T>();}
  if constexpr(std::is_same_v<T,PanelSettingsTask>){if(component_==Component::Settings)return settings()->settingsTask();}
  if constexpr(std::is_same_v<T,StateFieldService>){if(component_==Component::Name)return name();if(component_==Component::Playback&&playback()->component()==StatePlaybackModeService::Component::Ui)return playback()->ui();}
  if constexpr(std::is_same_v<T,StatePlaybackModeService>){if(component_==Component::Playback)return playback();}
  if constexpr(std::is_same_v<T,StateStoreDumpService>){if(component_==Component::Dump)return dump();}
  if constexpr(std::is_same_v<T,StateFactoryService>){if(component_==Component::Factory)return factory();}
  if constexpr(saveTaskType<T>()){if(component_==Component::Save)return save()->template task<T>();}
  if constexpr(fieldTaskType<T>()){if(component_==Component::Name)return name()->template task<T>();}
  if(component_==Component::Playback)return playback()->template task<T>();
  if(component_==Component::Dump)return dump()->template task<T>();
  if(component_==Component::Factory)return factory()->template task<T>();
  return nullptr;
 }

 static bool dataSpan(uint32_t a,unsigned n){return StateStoreService::dataSpan(a,n);}
 template<class Peek,class Resolver>static bool admitted(Kind kind,const Input&in,Peek&&peek,Resolver&&resolver){
  if(kind==Kind::Menu&&!in.pressed)return true;const auto wide=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(peek((a+n)&0xffffff));return v;};
  if(!dataSpan(wide(0x167bc),256)||!dataSpan(wide(0x167c0),256))return false;
  for(unsigned n=0;n<6;++n)if(!resolver.packing(wide(71306+4*n)))return false;
  if(!StateReloadService::admittedSchema(peek,[&](uint8_t c,uint32_t raw){return resolver.schema(c,raw);}))return false;
  return true;
 }
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;switch(component_){case Component::Menu:case Component::Page:return coordinator_.nextClocks(previous);case Component::Gate:return gate_->nextClocks(previous);case Component::Settings:return settings_->nextClocks(previous);case Component::Save:return save()->nextClocks(previous);case Component::Resolve:return resolve_->nextClocks(byteClock(0x167d6,previous));case Component::Name:return name_->nextClocks(previous);case Component::Playback:return playback_->nextClocks(previous);case Component::Action:return action_->nextClocks(byteClock(action_->stage()==StateScreenActionsTask::Stage::WriteEditLatch?0x16792:0x16784,previous));case Component::DumpNotice:return notice_->nextClocks();case Component::Dump:return dump_->nextClocks(previous);case Component::Factory:return factory_->nextClocks(previous);}return 0;}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolver,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  const auto childDone=[&](uint16_t presentation,bool accepted=false){summary_.presentation=presentation;if(!coordinator_.resume(presentation,accepted))throw std::logic_error("Invalid Store native child return");select(coordinator_.component()==StateStoreMenuCoordinator::Component::Menu?Component::Menu:Component::Page);};
  switch(component_){
   case Component::Menu:case Component::Page:coordinator_.advanceBefore(cost,read,write,previous);effect_=coordinator_.effect();if(coordinator_.request()!=StateStoreMenuCoordinator::Request::None)dispatch();break;
   case Component::Gate:gate_->advanceBefore(cost,read,write,resolver,previous);effect_={gate_->effect().read,gate_->effect().write};if(gate_->complete())childDone(summary_.presentation,gate_->summary().gate.accepted);break;
   case Component::Settings:settings_->advanceBefore(cost,read,write,resolver,previous);effect_={settings_->effect().read,settings_->effect().write};if(settings_->complete())childDone(uint16_t(settings_->flashTask()->target()));break;
   case Component::Save:{auto&s=*saves_[saveIndex_];s.advanceBefore(cost,read,write,resolver,previous);effect_={s.effect().read,s.effect().write};if(s.complete()){if(notice_&&notice_->awaitingChild()){notice_->resumeChild(0);select(Component::DumpNotice);}else if(notice_&&notice_->complete())childDone(uint16_t(s.tick()));else childDone(s.kind()==StateSaveService::Kind::Check?(s.summary().checkFlag?summary_.presentation:uint16_t(s.tick())):uint16_t(s.tick()),s.summary().checkFlag!=0);}break;}
   case Component::Resolve:{using T=StateBankNavigationTask;using S=T::Stage;const auto stage=resolve_->stage();resolve_->advanceBefore(cost,[&](T::Read q,uint32_t a){const auto v=uint8_t(read(q==T::Read::Part?0x16799:a,cost));if(q==T::Read::Part)summary_.selected=v;else summary_.enabled=v;return v;},[&](T::Write,uint32_t,uint8_t v){write(0x167d6,v,cost);},byteClock(0x167d6,previous));if(stage==S::ReturnPartMarker)effect_.read=true;if(resolve_->complete()){summary_.resolve=true;childDone(summary_.presentation);}break;}
   case Component::Name:name_->advanceBefore(cost,read,write,resolver,previous);effect_={name_->effect().read,name_->effect().write};if(name_->complete())childDone(name_->sceneEditRootSummary()->presentation);break;
   case Component::Playback:playback_->advanceBefore(cost,read,write,resolver,previous);effect_={playback_->effect().read,playback_->effect().write};if(playback_->complete())childDone(playback_->coordinator().presentation());break;
   case Component::Action:{using T=StateScreenActionsTask;const auto stage=action_->stage();action_->advanceBefore(cost,[](auto,auto)->uint32_t{throw std::logic_error("Unexpected store action read");},[&](T::Write q,uint16_t v){if(q!=T::Write::Edit&&q!=T::Write::Latch)throw std::logic_error("Unexpected store action write");write(q==T::Write::Latch?0x16792:0x16784,uint8_t(v),cost);},byteClock(stage==T::Stage::WriteEditLatch?0x16792:0x16784,previous));if(stage==T::Stage::ReturnAction||stage==T::Stage::ReturnEditClear)effect_.read=true;if(action_->complete())childDone(summary_.presentation);break;}
   case Component::DumpNotice:{using T=StateBrowseWorkflowTask;using S=T::Stage;const auto stage=notice_->stage();notice_->advanceBefore(cost,[&](T::Read q,uint32_t)->uint32_t{if(q!=T::Read::Global)throw std::logic_error("Unexpected store dump notice read");return uint8_t(read(0x169e1,cost));},[](auto,auto,auto){throw std::logic_error("Unexpected dump notice write");});if(stage==S::BindBrowseGlobal)summary_.presentation=uint16_t(0x68fc);if(stage==S::ReadBrowseGlobal)summary_.presentation=uint16_t((summary_.presentation&0xff00)|notice_->value());if(notice_->awaitingChild()){effect_.write=true;dump_.emplace(StateStoreDumpService::Kind::Select,StateStoreDumpService::Input{summary_.presentation});select(Component::Dump);}else if(notice_->complete()){if(notice_->exit()!=T::Exit::Notice)throw std::logic_error("Unexpected dump notice tail");startSave(StateSaveService::Kind::Notice,0x14edf);}break;}
   case Component::Dump:dump_->advanceBefore(cost,read,write,resolver,previous);effect_={dump_->effect().read,dump_->effect().write};if(dump_->complete()){if(!notice_->resumeChild(0))throw std::logic_error("Invalid dump notice continuation");select(Component::DumpNotice);}break;
   case Component::Factory:factory_->advanceBefore(cost,read,write,resolver,previous);effect_={factory_->effect().read,factory_->effect().write};if(factory_->complete())childDone(summary_.presentation);break;
  }return cost;
 }
private:
 template<class T>static constexpr bool saveTaskType(){return std::is_same_v<T,StateStoreUiTask>||std::is_same_v<T,StatePresetSaveTask>||std::is_same_v<T,StatePresetFormatTask>||std::is_same_v<T,StateProgramTask>||std::is_same_v<T,StateStoreServicesTask>||std::is_same_v<T,StateFlashCommitTask>||std::is_same_v<T,StateSavePageService>||std::is_same_v<T,StateMultiSaveTask>||std::is_same_v<T,StateGlobalSaveTask>||std::is_same_v<T,StateStoreService>||std::is_same_v<T,StateDisplayService>||std::is_same_v<T,PanelInitializeTask>||std::is_same_v<T,ScheduledTimerTask>||std::is_same_v<T,ControllerClockReadTask>||std::is_same_v<T,StateReloadService>||std::is_same_v<T,StateBankNavigationTask>||std::is_same_v<T,FlashPageProgramTask>||std::is_same_v<T,FlashPageCompareTask>;}
 template<class T>static constexpr bool fieldTaskType(){return std::is_same_v<T,StateFieldCoordinator>||std::is_same_v<T,StateApplyService>||std::is_same_v<T,StateDisplayService>||std::is_same_v<T,StateScreenService>||std::is_same_v<T,StateUiRefreshService>||std::is_same_v<T,StateStoreService>||std::is_same_v<T,StateFieldPacketService>||std::is_same_v<T,PanelSettingsLcdService>||std::is_same_v<T,StateSceneCoordinator>||std::is_same_v<T,StateSceneEditCoordinator>||std::is_same_v<T,StateVoiceRefreshService>;}
 static unsigned byteClock(uint32_t a,int p){return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0u:3u;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void startSave(StateSaveService::Kind k,uint32_t source=0){if(saveCount_==saves_.size())throw std::logic_error("Store save-outcome capacity");saveIndex_=saveCount_++;StateSaveService::Input in;in.source=source;in.noticeAction.value=0x55d4;in.saveAction.value=0x55e2;saves_[saveIndex_].emplace(k,in);select(Component::Save);}
 void dispatch(){using Q=StateStoreMenuCoordinator::Request;switch(coordinator_.request()){
  case Q::Gate:gate_.emplace(StateScreenService::Kind::Gate);select(Component::Gate);break;case Q::Coefficients:settings_.emplace(PanelSettingsService::Kind::Coefficients,PanelSettingsService::Input{summary_.presentation,0});select(Component::Settings);break;case Q::GlobalSave:startSave(StateSaveService::Kind::GlobalSave);break;case Q::Notice:startSave(StateSaveService::Kind::Notice,coordinator_.summary().noticeSource);break;case Q::Check:startSave(StateSaveService::Kind::Check);break;case Q::CommitMulti:startSave(StateSaveService::Kind::CommitMulti);break;case Q::CommitSingle:startSave(StateSaveService::Kind::CommitSingle);break;
  case Q::Resolve:resolve_.emplace(StateBankNavigationTask::Kind::Resolve);select(Component::Resolve);break;case Q::Name:name_.emplace(StateSceneEditCoordinator::Kind::Begin,StateSceneEditCoordinator::Input{summary_.presentation});select(Component::Name);break;case Q::Playback:playback_.emplace(StatePlaybackModeService::Kind::ResetPlayback,StatePlaybackModeService::Input{summary_.presentation});select(Component::Playback);break;
  case Q::ClearEdit:case Q::Release:action_.emplace(coordinator_.request()==Q::ClearEdit?StateScreenActionsTask::Kind::ClearEdit:StateScreenActionsTask::Kind::Return);select(Component::Action);break;case Q::Dump:notice_.emplace(StateBrowseWorkflowTask::Kind::Notice);select(Component::DumpNotice);break;case Q::Factory:factory_.emplace(StateFactoryService::Kind::Factory);select(Component::Factory);break;case Q::None:break;
 }}
 StateStoreMenuCoordinator coordinator_;Summary summary_{};Component component_=Component::Menu;uint32_t components_=0;Effect effect_{};std::optional<StateScreenService>gate_;std::optional<PanelSettingsService>settings_;std::array<std::optional<StateSaveService>,2>saves_{};unsigned saveIndex_=0,saveCount_=0;std::optional<StateBankNavigationTask>resolve_;std::optional<StateFieldService>name_;std::optional<StatePlaybackModeService>playback_;std::optional<StateScreenActionsTask>action_;std::optional<StateBrowseWorkflowTask>notice_;std::optional<StateStoreDumpService>dump_;std::optional<StateFactoryService>factory_;
};}
