#pragma once
#include "ksStateFactoryServicesTask.h"
#include "ksStateGlobalsTask.h"
#include "ksStateScreenActionsTask.h"
#include "ksStateSaveService.h"
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
// The factory page transaction includes the actual Flash commit, globals
// reload, notice and edit-context release. Every continuation is typed.
class StateFactoryService final {
public:
 enum class Kind {Provided,Factory,GlobalLoad};
 enum class Component {Factory,Save,Globals,Notice,ClearEdit};
 struct Input {uint32_t source=0;};
 struct Effect {bool read=false,write=false;};
 struct Summary {uint8_t factoryLast=0,globalsLast=0;bool saved=false,loaded=false,noticed=false,cleared=false;};
 explicit StateFactoryService(Kind k):StateFactoryService(k,Input{}){}
 StateFactoryService(Kind k,Input in):kind_(k),input_(in),factory_(k==Kind::Provided?StateFactoryServicesTask::Kind::Provided:StateFactoryServicesTask::Kind::Factory){if(k==Kind::Provided&&!dataSpan(in.source,256))throw std::logic_error("Unsupported native provided factory source");if(k==Kind::GlobalLoad){globals_.emplace(StateGlobalsTask::Kind::BootDefaults);select(Component::Globals);}else select(Component::Factory);}
 Kind kind()const{return kind_;}const Input&input()const{return input_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return complete_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 const StateFactoryServicesTask&factoryTask()const{return factory_;}const StateGlobalsTask*globalsTask()const{return globals_?&*globals_:nullptr;}
 const StateSaveService*save()const{return save_?&*save_:nullptr;}const StateSaveService*notice()const{return notice_?&*notice_:nullptr;}
 unsigned writePriority()const{return component_==Component::Save?save_->writePriority():component_==Component::Notice?notice_->writePriority():0;}bool critical()const{return writePriority()!=0;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,StateFactoryServicesTask>)return component_==Component::Factory?&factory_:nullptr;else if constexpr(std::is_same_v<T,StateGlobalsTask>)return component_==Component::Globals&&globals_?&*globals_:nullptr;else if constexpr(std::is_same_v<T,StateScreenActionsTask>)return component_==Component::ClearEdit&&clear_?&*clear_:nullptr;else if constexpr(std::is_same_v<T,StateSaveService>)return component_==Component::Save?save():component_==Component::Notice?notice():nullptr;else if constexpr(std::is_same_v<T,StateStoreUiTask>||std::is_same_v<T,StatePresetSaveTask>||std::is_same_v<T,StatePresetFormatTask>||std::is_same_v<T,StateProgramTask>||std::is_same_v<T,StateStoreServicesTask>||std::is_same_v<T,StateFlashCommitTask>||std::is_same_v<T,StateSavePageService>||std::is_same_v<T,FlashPageProgramTask>||std::is_same_v<T,FlashPageCompareTask>||std::is_same_v<T,StateMultiSaveTask>||std::is_same_v<T,StateGlobalSaveTask>||std::is_same_v<T,StateStoreService>||std::is_same_v<T,StateDisplayService>||std::is_same_v<T,PanelInitializeTask>||std::is_same_v<T,ScheduledTimerTask>||std::is_same_v<T,ControllerClockReadTask>||std::is_same_v<T,StateBankNavigationTask>||std::is_same_v<T,StateReloadService>){const auto*s=component_==Component::Save?save():component_==Component::Notice?notice():nullptr;return s?s->template task<T>():nullptr;}else return nullptr;}
 static bool dataSpan(uint32_t a,unsigned n){return StateStoreService::dataSpan(a,n);}
 static StateSaveService::Input saveInput(uint32_t source){return {source,0,0x81ff00,ScheduledTimerTask::ActionHandle{0x55d4},ScheduledTimerTask::ActionHandle{0x55e2}};}
 static StateSaveService::Input noticeInput(){return {0x14edf,0,0,ScheduledTimerTask::ActionHandle{0x55d4},ScheduledTimerTask::ActionHandle{0x55e2}};}
 template<class Peek>static bool admitted(Kind k,const Input&in,Peek&&){return k!=Kind::Provided||dataSpan(in.source,256);}
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;switch(component_){case Component::Factory:return factory_.nextClocks(byteClock(0x167fc+factory_.byteOffset(),previous));case Component::Globals:return globals_->nextClocks(byteClock(0x169e6+globals_->byteOffset(),previous));case Component::Save:return save_->nextClocks(previous);case Component::Notice:return notice_->nextClocks(previous);case Component::ClearEdit:return clear_->nextClocks(byteClock(clear_->stage()==StateScreenActionsTask::Stage::WriteEditLatch?0x16792:0x16784,previous));}return 0;}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolver,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  if(component_==Component::Factory){using T=StateFactoryServicesTask;const auto stage=factory_.stage();factory_.advanceBefore(cost,[&](unsigned i){return summary_.factoryLast=uint8_t(read(0x81fe00+i,cost-2));},[&](unsigned i,uint8_t v){write(0x167fc+i,v,cost);},byteClock(0x167fc+factory_.byteOffset(),previous));
   if(factory_.awaitingChild()||factory_.awaitingTail()){if(factory_.awaitingChild())effect_.write=true;switch(factory_.service()){case T::Service::Store:save_.emplace(StateSaveService::Kind::Offset,saveInput(kind_==Kind::Provided?input_.source:0x167fc));select(Component::Save);break;case T::Service::Load:globals_.emplace(StateGlobalsTask::Kind::BootDefaults);select(Component::Globals);break;case T::Service::Message:notice_.emplace(StateSaveService::Kind::Notice,noticeInput());select(Component::Notice);break;case T::Service::Panel:clear_.emplace(StateScreenActionsTask::Kind::ClearEdit);select(Component::ClearEdit);break;}}
   (void)stage;
  }else if(component_==Component::Globals){using T=StateGlobalsTask;const auto stage=globals_->stage();globals_->advanceBefore(cost,[&](T::Read q){if(q!=T::Read::Byte)throw std::logic_error("Unexpected globals factory read");return summary_.globalsLast=uint8_t(read(0x81ff00+globals_->byteOffset(),cost-2));},[&](T::Write q,uint8_t v){if(q!=T::Write::Byte)throw std::logic_error("Unexpected globals factory write");write(0x169e6+globals_->byteOffset(),v,cost);},byteClock(0x169e6+globals_->byteOffset(),previous));if(stage==T::Stage::ReturnCopy)effect_.read=true;if(globals_->complete()){summary_.loaded=true;if(kind_==Kind::GlobalLoad)complete_=true;else resume();}
  }else if(component_==Component::Save||component_==Component::Notice){const bool notice=component_==Component::Notice;auto&s=notice?*notice_:*save_;s.advanceBefore(cost,read,write,resolver,previous);effect_={s.effect().read,s.effect().write};if(s.complete()){if(notice)summary_.noticed=true;else summary_.saved=true;resume();}
  }else {using T=StateScreenActionsTask;clear_->advanceBefore(cost,[](T::Read,uint32_t)->uint32_t{throw std::logic_error("Unexpected factory release read");},[&](T::Write q,uint16_t v){if(q==T::Write::Edit)write(0x16784,uint8_t(v),cost);else if(q==T::Write::Latch)write(0x16792,uint8_t(v),cost);else throw std::logic_error("Unexpected factory release write");},byteClock(clear_->stage()==T::Stage::WriteEditLatch?0x16792:0x16784,previous));if(clear_->complete()){effect_.read=true;summary_.cleared=true;resume();}}
  return cost;
 }
private:
 static unsigned byteClock(uint32_t a,int p){a&=0xffffff;return a<0x400000&&!(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void resume(){if(factory_.awaitingTail()){factory_.retireTail();complete_=true;}else if(factory_.resumeChild())select(Component::Factory);else throw std::logic_error("Invalid factory child completion");}
 Kind kind_;Input input_;StateFactoryServicesTask factory_;Component component_=Component::Factory;uint32_t components_=0;bool complete_=false;Effect effect_{};Summary summary_{};std::optional<StateSaveService>save_,notice_;std::optional<StateGlobalsTask>globals_;std::optional<StateScreenActionsTask>clear_;
};
}
