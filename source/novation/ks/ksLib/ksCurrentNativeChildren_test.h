#pragma once
#include "ksRuntimeController.h"
namespace ks {
// Diagnostic traversal of the current semantic child, never cumulative coverage
// or a compatibility program counter. Tests still verify the real H8 location.
// A read-only current Save view. Standalone metrics remain standalone; nested
// children are exposed only while their actual owner is in Component::Save.
struct CurrentNativeSaveForTests {
 const StateSaveForeground*foreground=nullptr;const StateSaveService*nested=nullptr;
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
 if(menu&&menu->component()==StateStoreMenuService::Component::Save)return {nullptr,menu->save()};
 return {};
}
struct CurrentNativeChildrenForTests {
 CurrentNativeSaveForTests save{};
 const StateFieldService*field=nullptr;const StateScreenService*screen=nullptr;
 const StateDisplayService*display=nullptr;const StateApplyService*apply=nullptr;
 const StateMenuService*menu=nullptr;const StateEditAddressTask*address=nullptr;
};
inline CurrentNativeChildrenForTests currentNativeChildrenForTests(const RuntimeController&r){
 CurrentNativeChildrenForTests x;x.save=currentNativeSaveForTests(r);x.field=r.stateFieldForeground.service();if(!x.field)x.field=r.stateSceneForeground.service();if(!x.field)x.field=r.stateSceneEditForeground.service();if(!x.field)x.field=r.stateSceneToggleForeground.field();if(!x.field)if(const auto*s=r.stateEditActionForeground.service())x.field=s->ui();if(!x.field)if(const auto*s=r.statePlaybackModeForeground.service())if(s->component()==StatePlaybackModeService::Component::Ui)x.field=s->ui();if(!x.field)if(const auto*s=r.statePartNavigationForeground.service())if(s->component()==StatePartNavigationService::Component::UiRefresh||s->component()==StatePartNavigationService::Component::SceneEdit)x.field=s->field();x.screen=r.stateScreenForeground.service();if(!x.screen)x.screen=r.stateEditActionForeground.task<StateScreenService>();
 // Follow only current Menu continuations, never retained outcomes or masks.
 if(!x.field)x.field=r.stateStoreMenuForeground.task<StateFieldService>();
 if(!x.screen)x.screen=r.stateStoreMenuForeground.task<StateScreenService>();
 if(!x.display)x.display=r.stateStoreMenuForeground.task<StateDisplayService>();
 if(!x.apply)x.apply=r.stateStoreMenuForeground.task<StateApplyService>();
 if(x.field){if(const auto*s=x.field->task<StateScreenService>())x.screen=s;x.display=x.field->task<StateDisplayService>();x.apply=x.field->task<StateApplyService>();if(const auto*c=x.field->coordinator())x.address=c->task<StateEditAddressTask>();}
 if(x.screen){if(!x.display)x.display=x.screen->task<StateDisplayService>();if(!x.apply)x.apply=x.screen->task<StateApplyService>();x.menu=x.screen->task<StateMenuService>();}
 return x;
}
}
