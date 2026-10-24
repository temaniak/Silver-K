#pragma once
#include "ksStateReloadCoordinator.h"
#include "ksStateReloadVoiceService.h"
#include "ksStateReloadPanelService.h"
#include <optional>
namespace ks {
// Complete four-part reload, including voice release, live transport and the
// authenticated zero-request panel refresh. Every child is a native value task.
class StateReloadService final {
public:
 enum class Component {Coordinator,Voices,Signal,Panel};
 struct Effect {bool read=false,write=false;};
 Component component()const{return component_;}
 bool complete()const{return coordinator_.complete();}
 bool critical()const{return false;}
 const Effect&effect()const{return effect_;}
 const StateReloadCoordinator&coordinator()const{return coordinator_;}
 const StateReloadVoiceService*voices()const{return voices_?&*voices_:nullptr;}
 const StateReloadVoiceService*signal()const{return signal_?&*signal_:nullptr;}
 const StateReloadPanelService*panel()const{return panel_?&*panel_:nullptr;}
 uint32_t coordinatorComponents()const{return coordinator_.components();}
 uint32_t voiceComponents()const{return voiceComponents_;}
 uint32_t panelComponents()const{return panelComponents_;}
 template<class Peek,class Validator>static bool admittedSchema(Peek&&peek,Validator&&valid){return StateReloadPanelService::admittedSchema(peek,valid);}
 unsigned nextClocks(int previousWrite=-1)const{
  if(complete())return 0;
  switch(component_){case Component::Coordinator:return coordinator_.nextClocks(previousWrite);case Component::Voices:return voices_->nextClocks(previousWrite);case Component::Signal:return signal_->nextClocks(previousWrite);case Component::Panel:return panel_->nextClocks(previousWrite);}return 0;
 }
 template<class Reader,class Writer,class Resolver>
 unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,Resolver&&resolve,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;
  unsigned used=0;
  switch(component_){
  case Component::Coordinator:
   used=coordinator_.advanceBefore(cost,read,write,previousWrite);
   effect_={coordinator_.effect().read,coordinator_.effect().write};
   switch(coordinator_.request()){
   case StateReloadCoordinator::Request::Voices:voices_.emplace(StateReloadVoiceService::Kind::Voices);component_=Component::Voices;break;
   case StateReloadCoordinator::Request::Signal:signal_.emplace(StateReloadVoiceService::Kind::Signal);component_=Component::Signal;break;
   case StateReloadCoordinator::Request::Panel:panel_.emplace();component_=Component::Panel;break;
   case StateReloadCoordinator::Request::None:break;
   }
   break;
  case Component::Voices:
   used=voices_->advanceBefore(cost,read,write,previousWrite);effect_={voices_->effect().read,voices_->effect().write};voiceComponents_|=voices_->components();
   if(voices_->complete()){coordinator_.resumeEffects();component_=Component::Coordinator;}break;
  case Component::Signal:
   used=signal_->advanceBefore(cost,read,write,previousWrite);effect_={signal_->effect().read,signal_->effect().write};voiceComponents_|=signal_->components();
   if(signal_->complete()){coordinator_.resumeEffects();component_=Component::Coordinator;}break;
  case Component::Panel:
   used=panel_->advanceBefore(cost,read,write,[&](uint8_t category,uint32_t raw){return resolve(category,raw);},previousWrite);effect_={panel_->effect().read,panel_->effect().write};panelComponents_|=panel_->components();
   if(panel_->complete()){coordinator_.resumeEffects();component_=Component::Coordinator;}break;
  }
  return used;
 }
private:
 StateReloadCoordinator coordinator_;Component component_=Component::Coordinator;Effect effect_{};uint32_t voiceComponents_=0,panelComponents_=0;
 std::optional<StateReloadVoiceService>voices_,signal_;std::optional<StateReloadPanelService>panel_;
};
}
