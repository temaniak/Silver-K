#pragma once
#include "ksEditDecisionTask.h"
#include "ksEditAddressTask.h"
#include "ksStateFieldService.h"
#include "ksStateSaveService.h"
#include <optional>
#include <stdexcept>
namespace ks {
// Complete Scene edit-mode and commit transactions. Children observe the live
// resources at their own phases; completed records are domain facts only.
class StateSceneToggleService final {
public:
 enum class Kind {Toggle,Commit};
 enum class Component {Decision,Address,Scene,Name,Ui,Store,Check,Flash,Notice};
 enum class FieldRole {None,Backup,Load,Copy,Bank,Begin,Ui};
 enum class SaveRole {None,Check,Offset,Notice};
 struct Input {uint16_t event=0;ScheduledTimerTask::ActionHandle noticeAction{};};
 struct Effect {bool read=false,write=false;};
 struct Facts {uint8_t originalBank=0,originalMode=0;bool bankObserved=false,modeObserved=false,available=false,checkObserved=false,refresh=false,notice=false;uint32_t commitPage=0;};
 explicit StateSceneToggleService(Kind k):StateSceneToggleService(k,Input{}){}
 StateSceneToggleService(Kind k,Input in):kind_(k),input_(in){if(k==Kind::Toggle)decision_.emplace(EditDecisionTask::Kind::Toggle,uint8_t(in.event>>8),uint8_t(in.event));else address_.emplace(EditAddressTask::Kind::Commit);selectParent();}
 Kind kind()const{return kind_;}const Input&input()const{return input_;}const Facts&facts()const{return facts_;}
 bool complete()const{return done_;}Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}
 const EditDecisionTask*decision()const{return decision_?&*decision_:nullptr;}const EditAddressTask*address()const{return address_?&*address_:nullptr;}
 const StateFieldService*field()const{return field_?&*field_:nullptr;}const StateSaveService*save()const{return save_?&*save_:nullptr;}const StateStoreService*store()const{return store_?&*store_:nullptr;}
 FieldRole fieldRole()const{return fieldRole_;}SaveRole saveRole()const{return saveRole_;}
 const std::optional<StateSceneCoordinator::Summary>&backup()const{return backup_;}const std::optional<StateSceneCoordinator::Summary>&loaded()const{return loaded_;}const std::optional<StateSceneCoordinator::Summary>&copied()const{return copied_;}const std::optional<StateSceneCoordinator::Summary>&bank()const{return bank_;}
 const std::optional<StateSceneEditCoordinator::Summary>&begun()const{return begun_;}const std::optional<StateSaveService>&checked()const{return checked_;}const std::optional<StateSaveService>&committed()const{return committed_;}
 StateSaveService::Input saveInput()const{return saveInput_;}StateSaveService::Input checkInput()const{return checkInput_;}StateSaveService::Input commitInput()const{return commitInput_;}
 bool critical()const{return fieldRunning_&&field_->critical();}bool writePriority()const{return saveRunning_&&save_->writePriority();}
 template<class Peek>static bool admitted(Kind k,const Input&,Peek&&peek){
  if(k==Kind::Toggle&&!uint8_t(peek(0x1679e)))return true;
  if(k==Kind::Commit)return true;
  auto wide=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(peek(a+n));return v;};
  if(!uint8_t(peek(0x1679f))&&!StateSceneCoordinator::dataSpan(wide(0x1748e),64))return false;
  return StateFieldService::admitted(StateFieldService::Kind::Clear,{},peek);
 }
 unsigned nextClocks(int previous=-1)const{
  if(done_)return 0;if(fieldRunning_)return field_->nextClocks(previous);if(saveRunning_)return save_->nextClocks(previous);if(storeRunning_)return store_->nextClocks(previous);
  if(address_)return address_->nextClocks();return decision_->nextClocks(decisionCost(*decision_,previous));
 }
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned available,R&&read,W&&write,Resolver&&resolve,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>available)return 0;effect_={};
  if(fieldRunning_){
   field_->advanceBefore(cost,read,write,resolve,previous);effect_={field_->effect().read,field_->effect().write};
   if(field_->complete()){
    fieldRunning_=false;
    if(fieldRole_==FieldRole::Ui){done_=true;return cost;}
    if(fieldRole_==FieldRole::Begin){if(!field_->sceneEditRootSummary())throw std::logic_error("Missing Scene name-begin outcome");begun_=*field_->sceneEditRootSummary();}
    else {if(!field_->sceneRootSummary())throw std::logic_error("Missing Scene action outcome");const auto&s=*field_->sceneRootSummary();if(fieldRole_==FieldRole::Backup)backup_=s;else if(fieldRole_==FieldRole::Load)loaded_=s;else if(fieldRole_==FieldRole::Copy)copied_=s;else if(fieldRole_==FieldRole::Bank){bank_=s;const auto&t=s.calls[0].address;if(!address_->resumeAddress(t.page(),t.offset()))throw std::logic_error("Invalid Scene commit bank completion");facts_.commitPage=t.page();}}
    if(fieldRole_!=FieldRole::Bank&&!decision_->resumeChild())throw std::logic_error("Invalid Scene child completion");
    fieldRole_=FieldRole::None;selectParent();
   }return cost;
  }
  if(saveRunning_){
   save_->advanceBefore(cost,read,write,resolve,previous);effect_={save_->effect().read,save_->effect().write};
   if(save_->complete()){
    saveRunning_=false;
    if(saveRole_==SaveRole::Check){checked_=*save_;checkInput_=saveInput_;facts_.available=save_->summary().checkFlag!=0;facts_.checkObserved=true;if(!decision_->resumeAvailable(facts_.available))throw std::logic_error("Invalid Scene availability completion");}
    else if(saveRole_==SaveRole::Offset){committed_=*save_;commitInput_=saveInput_;address_.reset();if(kind_==Kind::Commit){done_=true;return cost;}if(!decision_->resumeChild())throw std::logic_error("Invalid Scene commit completion");}
    else {done_=true;return cost;}
    saveRole_=SaveRole::None;selectParent();
   }return cost;
  }
  if(storeRunning_){
   if(const auto*t=store_->nameTask())effect_.read=t->stage()==StatePatchNameTask::Stage::ReturnWrite;
   store_->advanceBefore(cost,read,write,resolve,previous);
   if(store_->complete()){storeRunning_=false;if(!decision_->resumeChild())throw std::logic_error("Invalid Scene name-copy completion");selectParent();}return cost;
  }
  const auto byte=[&](uint32_t a){if(!StateSceneCoordinator::dataSpan(a,1))throw std::logic_error("Unsupported Scene action read");return uint8_t(read(a,cost));};
  const auto put=[&](uint32_t a,uint8_t v){if(!StateSceneCoordinator::dataSpan(a,1))throw std::logic_error("Unsupported Scene action write");write(a,v,cost);};
  if(address_){
   address_->advanceBefore(cost,[](auto)->uint32_t{throw std::logic_error("Unexpected Scene commit read");},[](auto,auto){throw std::logic_error("Unexpected Scene commit write");},[](auto){return 3;});
   if(address_->awaitingChild()){effect_.write=true;startScene(FieldRole::Bank,StateSceneCoordinator::Kind::Bank);}
   else if(address_->handoff()){startSave(SaveRole::Offset,StateSaveService::Kind::Offset,{0x1738e,0,address_->page(),input_.noticeAction,{}});}
   return cost;
  }
  using S=EditDecisionTask::Stage;const auto stage=decision_->stage();
  decision_->advanceBefore(cost,[&](S s)->uint32_t{if(s==S::ToggleReadBank){facts_.bankObserved=true;return facts_.originalBank=byte(0x1679e);}if(s==S::ToggleReadMode){facts_.modeObserved=true;return facts_.originalMode=byte(0x1679f);}throw std::logic_error("Unsupported Scene toggle read");},[&](S s,uint32_t v){switch(s){case S::TogglePublishZeroMode:case S::TogglePublishOne:case S::TogglePublishTwo:case S::TogglePublishZero:put(0x1679f,uint8_t(v));break;case S::TogglePublishRow:put(0x17492,uint8_t(v));break;case S::TogglePublishZeroBank:put(0x1679e,uint8_t(v));break;default:throw std::logic_error("Unsupported Scene toggle write");}},[&](S){return decisionCost(*decision_,previous);});
  if(decision_->awaitingChild()){
   effect_.write=true;using C=EditDecisionTask::Child;switch(decision_->child()){
    case C::Available:startSave(SaveRole::Check,StateSaveService::Kind::Check,{});break;
    case C::Backup:startScene(FieldRole::Backup,StateSceneCoordinator::Kind::Backup);break;
    case C::Text:{StateSceneEditCoordinator::Input in;in.event=input_.event;field_.emplace(StateSceneEditCoordinator::Kind::Begin,in);fieldRole_=FieldRole::Begin;fieldRunning_=true;select(Component::Name);break;}
    case C::Load:startScene(FieldRole::Load,StateSceneCoordinator::Kind::Load);break;
    case C::Copy:startScene(FieldRole::Copy,StateSceneCoordinator::Kind::Copy);break;
    case C::RestoreParameters:store_.emplace(StateStoreService::Kind::WriteSelectedName);storeRunning_=true;select(Component::Store);break;
    case C::Commit:address_.emplace(EditAddressTask::Kind::Commit);select(Component::Address);break;
    default:throw std::logic_error("Unsupported Scene toggle child");
   }
  }else if(decision_->handoff()){
   if(decision_->route()==EditDecisionTask::Route::Refresh){StateUiRefreshService::Input in;in.event=in.character=input_.event;field_.emplace(StateUiRefreshService::Kind::Refresh,in);fieldRole_=FieldRole::Ui;fieldRunning_=true;facts_.refresh=true;select(Component::Ui);}
   else if(decision_->route()==EditDecisionTask::Route::NotifySaved){facts_.notice=true;startSave(SaveRole::Notice,StateSaveService::Kind::Notice,{0x150fc,0,0,input_.noticeAction,{}});}
   else throw std::logic_error("Unsupported Scene toggle tail");
  }else if(decision_->complete()){effect_.read=true;done_=true;}
  (void)stage;return cost;
 }
private:
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void selectParent(){select(address_?Component::Address:Component::Decision);}
 void startScene(FieldRole role,StateSceneCoordinator::Kind k){StateSceneCoordinator::Input in;in.event=input_.event;field_.emplace(k,in);fieldRole_=role;fieldRunning_=true;select(Component::Scene);}
 void startSave(SaveRole role,StateSaveService::Kind k,StateSaveService::Input in){in.noticeAction=input_.noticeAction;save_.emplace(k,in);saveInput_=in;saveRole_=role;saveRunning_=true;select(role==SaveRole::Check?Component::Check:role==SaveRole::Notice?Component::Notice:Component::Flash);}
 static unsigned decisionCost(const EditDecisionTask&t,int previous){using S=EditDecisionTask::Stage;uint32_t a=0;switch(t.stage()){case S::TogglePublishZeroMode:case S::TogglePublishOne:case S::TogglePublishTwo:case S::TogglePublishZero:a=0x1679f;break;case S::TogglePublishRow:a=0x17492;break;case S::TogglePublishZeroBank:a=0x1679e;break;default:return 3;}return !(previous&1)&&a==(uint32_t(previous)|1u)?0:3;}
 Kind kind_;Input input_;Facts facts_{};Effect effect_{};bool done_=false,fieldRunning_=false,saveRunning_=false,storeRunning_=false;FieldRole fieldRole_=FieldRole::None;SaveRole saveRole_=SaveRole::None;Component component_=Component::Decision;uint32_t components_=0;
 std::optional<EditDecisionTask>decision_;std::optional<EditAddressTask>address_;std::optional<StateFieldService>field_;std::optional<StateSaveService>save_,checked_,committed_;std::optional<StateStoreService>store_;
 std::optional<StateSceneCoordinator::Summary>backup_,loaded_,copied_,bank_;std::optional<StateSceneEditCoordinator::Summary>begun_;StateSaveService::Input saveInput_{},checkInput_{},commitInput_{};
};
}
