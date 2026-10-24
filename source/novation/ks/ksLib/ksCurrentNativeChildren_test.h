#pragma once
#include "ksRuntimeController.h"
namespace ks {
// Diagnostic traversal of the current semantic child, never cumulative coverage
// or a compatibility program counter. Tests still verify the real H8 location.
struct CurrentNativeChildrenForTests {
 const StateFieldService*field=nullptr;const StateScreenService*screen=nullptr;
 const StateDisplayService*display=nullptr;const StateApplyService*apply=nullptr;
 const StateMenuService*menu=nullptr;const StateEditAddressTask*address=nullptr;
};
inline CurrentNativeChildrenForTests currentNativeChildrenForTests(const RuntimeController&r){
 CurrentNativeChildrenForTests x;x.field=r.stateFieldForeground.service();if(!x.field)x.field=r.stateSceneForeground.service();if(!x.field)x.field=r.stateSceneEditForeground.service();if(!x.field)x.field=r.stateSceneToggleForeground.field();if(!x.field)if(const auto*s=r.stateEditActionForeground.service())x.field=s->ui();x.screen=r.stateScreenForeground.service();if(!x.screen)x.screen=r.stateEditActionForeground.task<StateScreenService>();
 if(x.field){if(const auto*s=x.field->task<StateScreenService>())x.screen=s;x.display=x.field->task<StateDisplayService>();x.apply=x.field->task<StateApplyService>();if(const auto*c=x.field->coordinator())x.address=c->task<StateEditAddressTask>();}
 if(x.screen){if(!x.display)x.display=x.screen->task<StateDisplayService>();if(!x.apply)x.apply=x.screen->task<StateApplyService>();x.menu=x.screen->task<StateMenuService>();}
 return x;
}
}
