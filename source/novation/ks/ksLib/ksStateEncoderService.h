#pragma once
#include "ksStateForegroundTask.h"
#include "ksPanelControlsService.h"
#include "ksStateScreenService.h"
#include "ksStateFieldService.h"
#include "ksStateProgramBrowserService.h"
#include <optional>
#include <variant>
#include <stdexcept>
#include <type_traits>
namespace ks {
// Complete encoder transaction: context test, physical delta and every edit
// route own their real typed descendants. Tail ownership has no extra return.
class StateEncoderService final {
public:
 enum class Kind {Encoder};
 enum class Component {Encoder,Context,Read,EditActive,Name,Category,Special,Field,Screen,Browser};
 struct Input {uint16_t presentation=0;};
 struct Effect {bool read=false,write=false;};
 struct Summary {uint8_t page=0,special=0,control=0;uint16_t encoder=0;bool read=false,editProbed=false,contextAccepted=false;Component tail=Component::Encoder;};
 StateEncoderService():StateEncoderService(Kind::Encoder,Input{}){}
 explicit StateEncoderService(Kind k):StateEncoderService(k,Input{}){}
 StateEncoderService(Kind k,Input in):kind_(k),input_(in){}
 Kind kind()const{return kind_;}const Input&input()const{return input_;}const Summary&summary()const{return summary_;}
 Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}
 bool complete()const{return complete_;}bool critical()const{if(const auto*t=field())return t->critical();if(const auto*t=screen())return t->critical();if(const auto*t=browser())return t->critical();return false;}
 const StateForegroundTask&encoderTask()const{return parent_;}
 const StateScreenService*context()const{return context_?&*context_:nullptr;}
 const PanelControlsService*reader()const{return reader_?&*reader_:nullptr;}
 const StateMenuService*probe()const{return probe_?&*probe_:nullptr;}
 const StateFieldService*field()const{return std::get_if<StateFieldService>(&tail_);}
 const StateScreenService*screen()const{return std::get_if<StateScreenService>(&tail_);}
 const StateProgramBrowserService*browser()const{return std::get_if<StateProgramBrowserService>(&tail_);}
 // Logical presentation word inherited by the next panel operation. This is
 // derived from completed semantic outcomes, never an active ABI shadow.
 uint16_t presentation()const{
  if(!summary_.read)return input_.presentation;
  if(summary_.tail==Component::Field){const auto&f=field()->summary();const auto&w=f.editWorkflow?*f.editWorkflow:*f.workflow;return w.input.event;}
  if(summary_.tail==Component::Name||summary_.tail==Component::Category||summary_.tail==Component::Special){const auto*s=field()->sceneEditRootSummary();if(!s)throw std::logic_error("Missing encoder name outcome");const auto owned=field()->projectionView().sceneEditView();if(s->uiObserved)return uiPresentation(owned);if(s->sceneObserved){const auto scene=owned.sceneView();const auto*o=scene.sceneRootSummary();if(o&&o->uiObserved)return uiPresentation(scene);}return s->presentation;}
  if(summary_.tail==Component::Screen){const auto level=screen()->summary().level;return uint16_t((unsigned(uint8_t((64u-level)*2+64))<<8)|level);}
  if(summary_.tail==Component::Browser)return browser()->summary().presentation;
  return summary_.encoder;
 }
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,StateForegroundTask>)return component_==Component::Encoder?&parent_:nullptr;
  else if constexpr(std::is_same_v<T,PanelEncoderTask>)return component_==Component::Read?reader_->encoderTask():nullptr;
  else if constexpr(std::is_same_v<T,PanelControlsService>)return component_==Component::Read?reader():nullptr;
  else if constexpr(std::is_same_v<T,StateFieldService>)return field();
  else if constexpr(std::is_same_v<T,StateProgramBrowserService>)return browser();
  else if constexpr(std::is_same_v<T,StateMenuService>)return component_==Component::EditActive?probe():nullptr;
  else if constexpr(std::is_same_v<T,StateScreenService>){if(component_==Component::Context)return context();if(screen())return screen();if(field())return field()->template task<T>();if(browser())return browser()->template task<T>();return nullptr;}
  else {
   if constexpr(std::is_same_v<T,StateScreenTask>||std::is_same_v<T,StateScreenEditTask>||std::is_same_v<T,StateEditContextTask>||std::is_same_v<T,StateApplyService>||std::is_same_v<T,StateDisplayService>||std::is_same_v<T,StateMenuService>||std::is_same_v<T,PanelSettingsService>){if(component_==Component::Context)return context_->template task<T>();if(screen())return screen()->template task<T>();}
   if constexpr(std::is_same_v<T,StateFieldCoordinator>||std::is_same_v<T,StateApplyService>||std::is_same_v<T,StateDisplayService>||std::is_same_v<T,StateScreenService>||std::is_same_v<T,StateUiRefreshService>||std::is_same_v<T,StateStoreService>||std::is_same_v<T,StateFieldPacketService>||std::is_same_v<T,PanelSettingsLcdService>||std::is_same_v<T,StateSceneCoordinator>||std::is_same_v<T,StateSceneEditCoordinator>||std::is_same_v<T,StateVoiceRefreshService>){if(field())return field()->template task<T>();}
   if(browser())return browser()->template task<T>();return nullptr;
  }
 }
 template<class Peek,class Resolver>static bool admitted(Kind,const Input&,Peek&&,Resolver&&){return true;}
 unsigned nextClocks(int previous=-1)const{
  if(complete_)return 0;
  if(component_==Component::Encoder)return parent_.nextClocks();
  if(component_==Component::Context)return context_->nextClocks(previous);
  if(component_==Component::Read)return reader_->nextClocks();
  if(component_==Component::EditActive)return probe_->nextClocks(previous);
  return std::visit([&](const auto&t)->unsigned{using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,std::monostate>)return 0;else return t.nextClocks(previous);},tail_);
 }
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolver,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};const auto owner=component_;
  if(owner==Component::Encoder){using S=StateForegroundTask::Stage;const auto stage=parent_.stage();
   parent_.advanceBefore(cost,[&](StateForegroundTask::Read r)->uint16_t{const auto at=r==StateForegroundTask::Read::Page?0x16790u:r==StateForegroundTask::Read::SpecialEdit?0x1679eu:0x16792u;return uint8_t(read(at,cost));},[](auto,auto){throw std::logic_error("Unexpected encoder parent write");});
   if(stage==S::ReadPage)summary_.page=parent_.page();if(stage==S::ReadSpecialEdit)summary_.special=parent_.special();if(stage==S::ReadControlEdit)summary_.control=parent_.control();
   if(parent_.awaitingChild()){
    effect_.write=true;
    if(stage==S::CheckEncoderContext){context_.emplace(StateScreenService::Kind::Gate);select(Component::Context);}
    else if(stage==S::ReadEncoder){reader_.emplace(PanelControlsService::Kind::Encoder);select(Component::Read);}
    else if(stage==S::CheckParameterEdit){probe_.emplace(StateMenuService::Kind::EditActive);select(Component::EditActive);}
    else throw std::logic_error("Unexpected encoder child request");
   }else if(parent_.complete()){
    if(stage==S::ReturnEncoder){effect_.read=true;complete_=true;}
    else if(stage==S::SelectNamePage)startScene(StateSceneEditCoordinator::Kind::AdjustName,Component::Name);
    else if(stage==S::SelectStorePage)startScene(StateSceneEditCoordinator::Kind::Category,Component::Category);
    else if(stage==S::SelectSpecialEdit)startScene(StateSceneEditCoordinator::Kind::Adjust,Component::Special);
    else if(stage==S::DispatchControlEdit){StateFieldService::Input in;in.event=parent_.event();in.action.value=0x5006;tail_.template emplace<StateFieldService>(StateFieldService::Kind::AutomaticEdit,in);startTail(Component::Field);}
    else if(stage==S::SelectParameterEdit){StateScreenService::Input in;in.character=parent_.event();tail_.template emplace<StateScreenService>(StateScreenService::Kind::Edit,in);startTail(Component::Screen);}
    else if(stage==S::DispatchParameterEdit){StateProgramBrowserService::Input in;in.event=parent_.event();tail_.template emplace<StateProgramBrowserService>(StateProgramBrowserService::Kind::Browse,in);startTail(Component::Browser);}
    else throw std::logic_error("Unexpected encoder tail decision");
   }
  }else if(owner==Component::Context){context_->advanceBefore(cost,read,write,resolver,previous);effect_={context_->effect().read,context_->effect().write};if(context_->complete()){summary_.contextAccepted=!context_->summary().gate.accepted;parent_.resumeChild(0,summary_.contextAccepted);select(Component::Encoder);}}
  else if(owner==Component::Read){const auto stage=reader_->encoderTask()->stage();reader_->advanceBefore(cost,read,write);effect_.read=stage==PanelEncoderTask::Stage::Return;if(reader_->complete()){const auto delta=reader_->delta();summary_.encoder=delta?uint16_t((unsigned(uint8_t(reader_->previousDelta()^delta))<<8)|reader_->encoderGain()):uint16_t(unsigned(reader_->quarter())<<8);summary_.read=true;parent_.resumeChild(summary_.encoder,false);select(Component::Encoder);}}
  else if(owner==Component::EditActive){effect_.read=probe_->gatePhase()==1;probe_->advanceBefore(cost,read,write,[](auto,unsigned){throw std::logic_error("Unexpected encoder timer action");},previous);if(probe_->complete()){summary_.editProbed=true;parent_.resumeChild(parent_.event(),probe_->edit()==0);select(Component::Encoder);}}
  else {std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;if constexpr(!std::is_same_v<T,std::monostate>){t.advanceBefore(cost,read,write,resolver,previous);effect_={t.effect().read,t.effect().write};if(t.complete())complete_=true;}},tail_);}
  components_|=1u<<unsigned(component_);return cost;
 }
private:
 static uint16_t renderedPresentation(const StateScreenService::Summary&s){auto word=s.labelWord;const auto&v=s.value;if(!v.observed||!v.type)return word;const auto display=[&](const StateScreenService::DisplayResult&d){using P=StateDisplayService::Presentation;if(d.presentation==P::Table||d.presentation==P::Text||d.presentation==P::Disabled||d.presentation==P::Character)word=d.output;};word=uint16_t((word&0xff00)|145);display(v.currentDisplay);if(v.current==v.reference)display(v.matchDisplay);else{word=uint16_t((word&0xff00)|209);display(v.referenceDisplay);}return word;}
 template<class View>static uint16_t uiPresentation(const View&view){const auto*ui=view.lastRefresh();if(!ui)throw std::logic_error("Missing encoder UI presentation");if(ui->automaticIndex!=~0u){const auto child=view.completedView(ui->automaticIndex);const auto&w=child.summary().editWorkflow?child.summary().editWorkflow:child.summary().workflow;if(!w)throw std::logic_error("Missing encoder automatic presentation");return w->input.event;}if(ui->rendered){const auto*s=view.lastScreen();if(!s)throw std::logic_error("Missing encoder rendered presentation");return renderedPresentation(*s);}return ui->character;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void startTail(Component c){summary_.tail=c;select(c);}
 void startScene(StateSceneEditCoordinator::Kind k,Component c){StateSceneEditCoordinator::Input in;in.event=parent_.event();tail_.template emplace<StateFieldService>(k,in);startTail(c);}
 Kind kind_;Input input_;Summary summary_{};Effect effect_{};StateForegroundTask parent_{StateForegroundTask::Kind::Encoder};Component component_=Component::Encoder;uint32_t components_=1;bool complete_=false;
 std::optional<StateScreenService>context_;std::optional<PanelControlsService>reader_;std::optional<StateMenuService>probe_;std::variant<std::monostate,StateFieldService,StateScreenService,StateProgramBrowserService>tail_;
};
}
