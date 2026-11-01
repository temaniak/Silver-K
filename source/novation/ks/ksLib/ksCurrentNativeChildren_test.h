#pragma once
#include "ksRuntimeController.h"
namespace ks {
// Test-only typed Main traversal. A retained completed child is never a selector.
inline const NativeServiceCycle*currentNativeMainForTests(const RuntimeController&r){return r.serviceCycleForeground.service();}
inline const StateCentralActionsService*currentNativeCentralForTests(const RuntimeController&r){
 const auto*m=currentNativeMainForTests(r);if(!m||m->external()!=NativeServiceCycle::External::None)return nullptr;
 if(const auto*ui=m->current<StateCentralActionsService>())return !ui->complete()?ui:nullptr;
 if(const auto*t=m->current<ScheduledTimerService>()){const auto*ui=t->task<StateCentralActionsService>();return ui&&!ui->complete()?ui:nullptr;}
 return nullptr;
}
struct CurrentNativeParentForTests{
 const uint64_t*started=nullptr;const uint64_t*completed=nullptr;
 explicit operator bool()const{return started&&completed;}
 uint64_t job()const{return started?*started:0;}uint64_t returns()const{return completed?*completed:0;}
};
inline CurrentNativeParentForTests currentNativeMainParentForTests(const RuntimeController&r){
 const auto*m=currentNativeMainForTests(r);return m?CurrentNativeParentForTests{&r.serviceCycleForeground.started,&r.serviceCycleForeground.children[m->childIndex()]}:CurrentNativeParentForTests{};
}
struct ScopedNativeChildWitnessForTests{
 bool observed=false,returned=false;CurrentNativeParentForTests parent{};uint64_t job=0,returnsBefore=0;
 void observe(CurrentNativeParentForTests candidate,bool activeChild){
  if(observed&&!returned){if(parent.returns()>returnsBefore)returned=true;else if(parent.job()!=job)throw std::logic_error("Observed semantic child changed parent before returning");}
  if(!observed&&candidate&&activeChild){observed=true;parent=candidate;job=parent.job();returnsBefore=parent.returns();}
 }
};
struct CurrentNativeTickForTests{
 const StateTickForeground*standalone=nullptr;const ServiceCycleForeground*main=nullptr;
 const StateTickService*service()const{return main?(main->service()?main->service()->current<StateTickService>():nullptr):standalone?standalone->service():nullptr;}
 bool active()const{return service()!=nullptr;}
 uint64_t started()const{return main?main->tickReturns.started:standalone->started;}
 uint64_t completed()const{return main?main->tickReturns.completed:standalone->completed;}
 uint64_t returnedButtons()const{return main?main->tickReturns.buttons:standalone->returnedButtons;}
 uint64_t returnedAdc()const{return main?main->tickReturns.adc:standalone->returnedAdc;}
 uint64_t returnedEncoder()const{return main?main->tickReturns.encoder:standalone->returnedEncoder;}
 uint64_t returnedPedal()const{return main?main->tickReturns.pedal:standalone->returnedPedal;}
 uint64_t returnedOutput()const{return main?main->tickReturns.output:standalone->returnedOutput;}
};
inline CurrentNativeTickForTests currentNativeTickForTests(const RuntimeController&r){
 if(r.stateTickForeground.active())return {&r.stateTickForeground,nullptr};
 if(r.serviceCycleForeground.active()||r.serviceCycleForeground.started||r.serviceCycleForeground.initial)return {nullptr,&r.serviceCycleForeground};
 return {&r.stateTickForeground,nullptr};
}
// Diagnostic traversal of the current semantic child, never cumulative coverage
// or a compatibility program counter. Tests still verify the real H8 location.
// A read-only current Save view. Standalone metrics remain standalone; nested
// children are exposed only while their actual owner is in Component::Save.
struct CurrentNativeSaveForTests {
 const StateSaveForeground*foreground=nullptr;const StateSaveService*nested=nullptr;CurrentNativeParentForTests parent{};
 explicit operator bool()const{return foreground||nested;}
 bool at(StateSaveService::Kind k)const{return nested?nested->kind()==k:foreground&&foreground->at(k);}
 bool at(StateSaveService::Component c)const{return nested?nested->component()==c:foreground&&foreground->at(c);}
 template<class T>const T*task()const{return nested?nested->template task<T>():foreground?foreground->template task<T>():nullptr;}
 const StateStoreService*preparation()const{return task<StateStoreService>();}
 const StateSavePageService*page()const{return task<StateSavePageService>();}
 const StateReloadService*reload()const{return task<StateReloadService>();}
 bool atPreparation(StateStoreService::Kind k)const{const auto*p=preparation();return p&&p->kind()==k;}
 bool atPreparation(StateStoreService::Component c)const{const auto*p=preparation();return p&&p->component()==c;}
 bool atPage(StateSavePageService::Component c)const{const auto*p=page();return p&&p->component()==c;}
 bool atReload(StateReloadService::Component c)const{const auto*p=reload();return p&&p->component()==c;}
 bool at(FlashPageProgramTask::Stage s)const{const auto*p=task<FlashPageProgramTask>();return p&&p->stage()==s;}
 bool at(FlashPageCompareTask::Stage s)const{const auto*p=task<FlashPageCompareTask>();return p&&p->stage()==s;}
};
inline CurrentNativeSaveForTests currentNativeSaveForTests(const RuntimeController&r){
 if(r.stateSaveForeground.active())return {&r.stateSaveForeground,nullptr};
 const auto*menu=r.stateStoreMenuForeground.service();
 if(menu&&menu->component()==StateStoreMenuService::Component::Save)return {nullptr,menu->save(),{&r.stateStoreMenuForeground.started,&r.stateStoreMenuForeground.completed}};
 const auto*ui=currentNativeCentralForTests(r);const auto*m=currentNativeMainForTests(r);if(ui&&m){if(const auto*save=ui->task<StateSaveService>())return {nullptr,save,{&r.serviceCycleForeground.started,&r.serviceCycleForeground.children[m->childIndex()]}};}
 return {};
}
struct CurrentNativeChildrenForTests {
 CurrentNativeSaveForTests save{};
 const StateFieldService*field=nullptr;const StateScreenService*screen=nullptr;
 const StateDisplayService*display=nullptr;const StateApplyService*apply=nullptr;
 const StateMenuService*menu=nullptr;const StateEditAddressTask*address=nullptr;
 // Current active upper-panel ownership, separate from retained outcomes.
 bool menuPrevious=false,menuNext=false;const StateMenuActionsTask*menuAction=nullptr;const ControllerClockReadTask*menuClock=nullptr;const ScheduledTimerTask*menuRepeat=nullptr;
 const StateCentralActionsService*central=nullptr;const StateEventsService*events=nullptr;const StatePanelService*panel=nullptr;const StateStoreMenuService*storeMenu=nullptr;
 const StateTickService*tick=nullptr;const ControllerClockReadTask*tickClock=nullptr;const StateEncoderService*encoder=nullptr;
 const PanelScanService*panelScan=nullptr;const PanelControlsService*panelControls=nullptr;
 const PanelPedalOutputService*pedalOutput=nullptr;const SerialForegroundService*pedalSerial=nullptr;const SerialForegroundService*pumpSerial=nullptr;
};
inline CurrentNativeChildrenForTests currentNativeChildrenForTests(const RuntimeController&r){
 CurrentNativeChildrenForTests x;
 x.tick=currentNativeTickForTests(r).service();
 if(const auto*m=currentNativeMainForTests(r))if(const auto*p=m->current<NativeMidiPumpService>())if(p->component()==NativeMidiPumpService::Component::Serial)x.pumpSerial=p->task<SerialForegroundService>();
 // A resumed native tail may own the real Pump outside Main. Its typed getter follows only the current child.
 if(!x.pumpSerial)x.pumpSerial=r.nativeMidiPipelineForeground.task<SerialForegroundService>();
 if(const auto*m=currentNativeMainForTests(r))x.panel=m->current<StatePanelService>();
 x.storeMenu=r.stateStoreMenuForeground.service();
 x.central=currentNativeCentralForTests(r);if(x.central){if(const auto*e=x.central->task<StateEventsService>())x.events=e;if(const auto*m=x.central->task<StateStoreMenuService>())x.storeMenu=m;}
 if(x.tick){
  x.tickClock=x.tick->task<ControllerClockReadTask>();
  switch(x.tick->component()){
  case StateTickService::Component::Buttons:x.panelScan=x.tick->buttons();break;
  case StateTickService::Component::Adc:x.panelControls=x.tick->adc();break;
  case StateTickService::Component::Encoder:x.encoder=x.tick->encoder();break;
  case StateTickService::Component::Pedal:x.panelControls=x.tick->pedal();break;
  case StateTickService::Component::Output:x.pedalOutput=x.tick->output();break;
  default:break;
  }
 }else{x.encoder=r.stateEncoderForeground.service();x.pedalOutput=r.panelPedalOutputForeground.service();}
 if(x.encoder&&x.encoder->component()==StateEncoderService::Component::Read)x.panelControls=x.encoder->reader();
 if(x.pedalOutput&&x.pedalOutput->component()==PanelPedalOutputService::Component::Serial)x.pedalSerial=x.pedalOutput->task<SerialForegroundService>();
 x.save=currentNativeSaveForTests(r);x.field=r.stateFieldForeground.service();if(!x.field)x.field=r.stateSceneForeground.service();if(!x.field)x.field=r.stateSceneEditForeground.service();if(!x.field)x.field=r.stateSceneToggleForeground.field();if(!x.field)if(const auto*s=r.stateEditActionForeground.service())x.field=s->ui();if(!x.field)if(const auto*s=r.statePlaybackModeForeground.service())if(s->component()==StatePlaybackModeService::Component::Ui)x.field=s->ui();if(!x.field)if(const auto*s=r.statePartNavigationForeground.service())if(s->component()==StatePartNavigationService::Component::UiRefresh||s->component()==StatePartNavigationService::Component::SceneEdit)x.field=s->field();x.screen=r.stateScreenForeground.service();if(!x.screen)x.screen=r.stateEditActionForeground.task<StateScreenService>();
 if(x.central){
  x.menuAction=x.central->coordinator().task<StateMenuActionsTask>();
  if(x.menuAction){const auto&c=x.central->coordinator();for(unsigned n=0;n<c.nodeCount();++n){const auto&node=c.node(n);if(std::get_if<StateMenuActionsTask>(&node.task)==x.menuAction){x.menuPrevious=node.kind==StateCentralActionsCoordinator::Kind::MenuPrevious;x.menuNext=node.kind==StateCentralActionsCoordinator::Kind::MenuNext;break;}}}
  if(x.central->component()==StateCentralActionsService::Component::Repeat&&x.central->childInput().action==StateCentralActionsService::ActionKind::MenuPrevious){x.menuClock=x.central->task<ControllerClockReadTask>();x.menuRepeat=x.central->task<ScheduledTimerTask>();}
  if(!x.field)x.field=x.central->task<StateFieldService>();
  if(!x.screen)x.screen=x.central->task<StateScreenService>();
  if(!x.display)x.display=x.central->task<StateDisplayService>();
  if(!x.apply)x.apply=x.central->task<StateApplyService>();
  if(!x.menu)x.menu=x.central->task<StateMenuService>();
 }
 // Follow only current Encoder/Browser children. Its scoped Field outcomes
 // stay in their owning graph; retained completed children are not selectors.
 if(x.encoder){
  if(!x.field)x.field=x.encoder->task<StateFieldService>();
  if(!x.screen)x.screen=x.encoder->task<StateScreenService>();
  if(!x.display)x.display=x.encoder->task<StateDisplayService>();
  if(!x.apply)x.apply=x.encoder->task<StateApplyService>();
  if(!x.address)if(const auto*c=x.encoder->task<StateFieldCoordinator>())x.address=c->task<StateEditAddressTask>();
 }
 // Follow only current Menu continuations, never retained outcomes or masks.
 if(!x.field)x.field=r.stateStoreMenuForeground.task<StateFieldService>();
 if(!x.screen)x.screen=r.stateStoreMenuForeground.task<StateScreenService>();
 if(!x.display)x.display=r.stateStoreMenuForeground.task<StateDisplayService>();
 if(!x.apply)x.apply=r.stateStoreMenuForeground.task<StateApplyService>();
 if(x.field){if(const auto*s=x.field->task<StateScreenService>())x.screen=s;x.display=x.field->task<StateDisplayService>();x.apply=x.field->task<StateApplyService>();if(const auto*c=x.field->coordinator())x.address=c->task<StateEditAddressTask>();}
 if(x.screen){if(!x.display)x.display=x.screen->task<StateDisplayService>();if(!x.apply)x.apply=x.screen->task<StateApplyService>();if(const auto*m=x.screen->task<StateMenuService>())x.menu=m;}
 return x;
}
}
