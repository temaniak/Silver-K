#pragma once
#include <cstdint>
namespace ks {
// Single/Multi/playback screen lifecycle, complete UI reset, program step/edit toggles and patch-bit actions.
class StateScreenActionsTask final {
public:
 enum class Kind { Return, Single, Playback, ResetPlayback, ClearEdit, ClearUi, Multi, Down, Up, Step, ToggleEdit, SelectEdit, ToggleOne, ToggleTwo };enum class Stage { ReturnAction, SelectSingle, ReadSingleEditPart, SaveSinglePart, ResetSingleUi, RestoreSinglePart, ReadSingleMode, SelectSingleMode, BindSingleMode, WriteSingleMode, TestSinglePart, SelectSinglePart, ClearSingleAddress, ReadSinglePart, ScaleSinglePart, ReadSinglePreset, WriteSinglePreset, RefreshSingleRouting, MarkSingleReload, WriteSingleReload, LoadSingleProgram, RefreshSingleScreen, ResetSingleVoices, ClearPlaybackPage, WritePlaybackPage, ResetPlaybackContext, ClearPlaybackEdit, RefreshPlaybackScreen, ResetPlaybackVoices, ClearEditValue, WriteEditActive, WriteEditLatch, ReturnEditClear, ClearUiValue, WriteUiLast, WriteUiSpecial, WriteUiAuxiliary, WriteUiOther, WriteUiEdit, WriteUiLatch, WriteUiMulti, WriteUiPage, WriteUiPart, WriteUiTranspose, WriteUiNote, ShowClearedUi, SelectMulti, ResetMultiUi, ReadMultiCurrent, WriteMultiRequest, ReadMultiHeld, WriteMultiHeld, ReadMultiMode, BindMultiMode, WriteMultiMode, RefreshMultiRouting, TestMultiMode, SelectMultiMode, LoadMultiProgram, ResolveMultiParts, RefreshResolvedMulti, RefreshMultiScreen, ResetMultiVoices, SelectStepDown, BindStepDown, JoinStepDown, SelectStepUp, BindStepUp, ReadStepPage, TestStepName, SelectStepName, TestStepPanel, SelectStepPanel, ReadStepSpecial, SelectStepSpecial, ReadStepMode, SelectStepMode, ReadStepMulti, SelectStepMulti, EnterMultiStep, ApplyProgramStep, ReturnProgramStep, SelectEditToggle, ClearEditToggleLatch, WriteEditToggleLatch, ReadEditTogglePage, SelectEditTogglePage, ResetEditToggleContext, ReadEditToggleValue, ToggleEditValue, WriteEditToggleValue, ObserveEditToggleGate, SelectEditToggleGate, ReloadEditCalibration, RefreshEditToggle, TestEditEvent, SelectEditEvent, ClearEditEventLatch, WriteEditEventLatch, ReadEditEventPage, SelectEditEventPage, ResetEditEventContext, ReadEditEventScreen, CompareEditEventScreen, SelectEditEventScreen, BindEditEventOn, WriteEditEventOn, JoinEditEvent, ReadEditEventActive, ToggleEditEventActive, WriteEditEventActive, SelectEditEventActive, ApplyEditEvent, RefreshEditEvent, SelectPatchToggleOne, ClearPatchToggleOne, BindPatchToggleOne, ReadPatchToggleOneBase, LocatePatchToggleOne, ReadPatchToggleOne, FlipPatchToggleOne, ApplyPatchToggleOne, SelectPatchToggleTwo, ClearPatchToggleTwo, BindPatchToggleTwo, ReadPatchToggleTwoBase, LocatePatchToggleTwo, ReadPatchToggleTwo, FlipPatchToggleTwo, ApplyPatchToggleTwo, AwaitChild, Complete };
 enum class Read { EditPart, Mode, ActivePart, Current, Held, UiPage, Special, Multi, Edit, Screen, Preset, PatchBase, PatchByte };enum class Write { Mode, Preset, Reload, UiPage, Edit, Latch, Request, HeldRequest, Clear167fb, Clear1679e, Clear167a0, Clear1679f, Clear16784, Clear16792, Clear167d5, Clear16790, Clear16799, Clear167a3, Clear16783 };enum class Exit { None, Release, ResetVoices, ShowUi, NameStep, PanelStep, SpecialStep, MultiStep, Refresh, ApplyPatch };Exit exit()const{return exit_;}uint32_t active()const{return active_;}uint32_t part()const{return part_;}uint32_t multi()const{return multi_;}uint32_t patch()const{return patch_;}uint32_t held()const{return held_;}uint32_t mask()const{return mask_;}uint32_t page()const{return page_;}uint32_t mode()const{return mode_;}uint32_t value()const{return value_;}uint32_t special()const{return special_;}uint32_t screen()const{return screen_;}uint32_t edit()const{return edit_;}uint32_t preset()const{return preset_;}
 explicit StateScreenActionsTask(Kind kind,uint16_t event=0,bool pressed=true){event_=event;pressed_=pressed;switch(kind){case Kind::Return:stage_=Stage::ReturnAction;break;case Kind::Single:stage_=Stage::SelectSingle;break;case Kind::Playback:stage_=Stage::ClearPlaybackPage;break;case Kind::ResetPlayback:stage_=Stage::ResetPlaybackContext;break;case Kind::ClearEdit:stage_=Stage::ClearEditValue;break;case Kind::ClearUi:stage_=Stage::ClearUiValue;break;case Kind::Multi:stage_=Stage::SelectMulti;break;case Kind::Down:stage_=Stage::SelectStepDown;break;case Kind::Up:stage_=Stage::SelectStepUp;break;case Kind::Step:stage_=Stage::ReadStepPage;break;case Kind::ToggleEdit:stage_=Stage::SelectEditToggle;break;case Kind::SelectEdit:stage_=Stage::TestEditEvent;break;case Kind::ToggleOne:stage_=Stage::SelectPatchToggleOne;break;case Kind::ToggleTwo:stage_=Stage::SelectPatchToggleTwo;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(bool nonzero){if(!awaitingChild())return false;childNonzero_=nonzero;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ReturnAction:case Stage::ReturnEditClear:case Stage::ReturnProgramStep:return 17; case Stage::ReadSingleEditPart:case Stage::ReadSingleMode:case Stage::ReadSinglePart:case Stage::WriteSinglePreset:case Stage::ReadMultiCurrent:case Stage::ReadMultiHeld:case Stage::ReadMultiMode:case Stage::ReadStepPage:case Stage::ReadStepSpecial:case Stage::ReadStepMode:case Stage::ReadStepMulti:case Stage::ReadEditTogglePage:case Stage::ReadEditToggleValue:case Stage::ReadEditEventPage:case Stage::ReadEditEventScreen:case Stage::ReadEditEventActive:return 12; case Stage::SaveSinglePart:case Stage::RestoreSinglePart:return 11; case Stage::ResetSingleUi:case Stage::ReadSinglePreset:case Stage::ClearPlaybackEdit:case Stage::ResetMultiUi:return 15; case Stage::WriteSingleMode:case Stage::WriteSingleReload:case Stage::WritePlaybackPage:case Stage::WriteEditActive:case Stage::WriteEditLatch:case Stage::WriteUiLast:case Stage::WriteUiSpecial:case Stage::WriteUiAuxiliary:case Stage::WriteUiOther:case Stage::WriteUiEdit:case Stage::WriteUiLatch:case Stage::WriteUiMulti:case Stage::WriteUiPage:case Stage::WriteUiPart:case Stage::WriteUiTranspose:case Stage::WriteUiNote:case Stage::WriteMultiRequest:case Stage::WriteMultiHeld:case Stage::WriteMultiMode:case Stage::WriteEditToggleLatch:case Stage::WriteEditToggleValue:case Stage::WriteEditEventLatch:case Stage::WriteEditEventOn:case Stage::WriteEditEventActive:return 9+store; case Stage::RefreshSingleRouting:case Stage::LoadSingleProgram:case Stage::RefreshSingleScreen:case Stage::ResetPlaybackContext:case Stage::RefreshPlaybackScreen:case Stage::RefreshMultiRouting:case Stage::LoadMultiProgram:case Stage::ResolveMultiParts:case Stage::RefreshResolvedMulti:case Stage::RefreshMultiScreen:case Stage::ApplyProgramStep:case Stage::ResetEditToggleContext:case Stage::ObserveEditToggleGate:case Stage::ReloadEditCalibration:case Stage::ResetEditEventContext:case Stage::ApplyEditEvent:return 20; case Stage::ResetSingleVoices:case Stage::ResetPlaybackVoices:case Stage::ShowClearedUi:case Stage::SelectMulti:case Stage::ResetMultiVoices:case Stage::SelectStepDown:case Stage::SelectStepUp:case Stage::SelectStepName:case Stage::SelectStepPanel:case Stage::SelectStepSpecial:case Stage::EnterMultiStep:case Stage::SelectEditToggle:case Stage::SelectEditTogglePage:case Stage::SelectEditToggleGate:case Stage::RefreshEditToggle:case Stage::SelectEditEvent:case Stage::SelectEditEventPage:case Stage::SelectEditEventActive:case Stage::RefreshEditEvent:case Stage::SelectPatchToggleOne:case Stage::ApplyPatchToggleOne:case Stage::SelectPatchToggleTwo:case Stage::ApplyPatchToggleTwo:return 8; case Stage::ReadPatchToggleOneBase:case Stage::ReadPatchToggleTwoBase:return 18; case Stage::ReadPatchToggleOne:case Stage::ReadPatchToggleTwo:return 6; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::SelectSingle:if(!pressed_)finish(Exit::Release);else next();break;
case Stage::ReadSingleEditPart:part_=uint8_t(read(Read::EditPart,0));next();break;
case Stage::ReadSingleMode:mode_=uint8_t(read(Read::Mode,0));next();break;
case Stage::SelectSingleMode:stage_=mode_?Stage::BindSingleMode:Stage::RefreshSingleScreen;break;
case Stage::WriteSingleMode:write(Write::Mode,0);next();break;
case Stage::SelectSinglePart:stage_=part_?Stage::ClearSingleAddress:Stage::RefreshSingleRouting;break;
case Stage::ReadSinglePart:active_=uint8_t(read(Read::ActivePart,0));next();break;
case Stage::ScaleSinglePart:index_=uint8_t(active_<<1);next();break;
case Stage::ReadSinglePreset:preset_=uint16_t(read(Read::Preset,index_));next();break;
case Stage::WriteSinglePreset:write(Write::Preset,preset_);next();break;
case Stage::WriteSingleReload:write(Write::Reload,1);next();break;
case Stage::ResetSingleVoices:finish(Exit::ResetVoices);break;
case Stage::WritePlaybackPage:write(Write::UiPage,0);next();break;
case Stage::ResetPlaybackVoices:finish(Exit::ResetVoices);break;
case Stage::WriteEditActive:write(Write::Edit,0);next();break;
case Stage::WriteEditLatch:write(Write::Latch,0);next();break;
case Stage::ShowClearedUi:finish(Exit::ShowUi);break;
case Stage::SelectMulti:if(!pressed_)finish(Exit::Release);else next();break;
case Stage::ReadMultiCurrent:mask_=uint8_t(read(Read::Current,0));next();break;
case Stage::WriteMultiRequest:write(Write::Request,mask_);next();break;
case Stage::ReadMultiHeld:held_=uint8_t(read(Read::Held,0));next();break;
case Stage::WriteMultiHeld:write(Write::HeldRequest,held_);next();break;
case Stage::ReadMultiMode:mode_=uint8_t(read(Read::Mode,0));next();break;
case Stage::WriteMultiMode:write(Write::Mode,1);next();break;
case Stage::SelectMultiMode:stage_=mode_?Stage::ResolveMultiParts:Stage::LoadMultiProgram;break;
case Stage::ResetMultiVoices:finish(Exit::ResetVoices);break;
case Stage::SelectStepDown:if(!pressed_)finish(Exit::Release);else next();break;
case Stage::BindStepDown:delta_=255;next();break;
case Stage::JoinStepDown:stage_=Stage::ReadStepPage;break;
case Stage::SelectStepUp:if(!pressed_)finish(Exit::Release);else next();break;
case Stage::BindStepUp:delta_=1;next();break;
case Stage::ReadStepPage:page_=uint8_t(read(Read::UiPage,0));next();break;
case Stage::SelectStepName:if(page_==2)finish(Exit::NameStep);else next();break;
case Stage::SelectStepPanel:if(page_==3)finish(Exit::PanelStep);else next();break;
case Stage::ReadStepSpecial:special_=uint8_t(read(Read::Special,0));next();break;
case Stage::SelectStepSpecial:if(special_)finish(Exit::SpecialStep);else next();break;
case Stage::ReadStepMode:mode_=uint8_t(read(Read::Mode,0));next();break;
case Stage::SelectStepMode:stage_=mode_?Stage::ReadStepMulti:Stage::ApplyProgramStep;break;
case Stage::ReadStepMulti:multi_=uint8_t(read(Read::Multi,0));next();break;
case Stage::SelectStepMulti:stage_=multi_?Stage::ApplyProgramStep:Stage::EnterMultiStep;break;
case Stage::EnterMultiStep:finish(Exit::MultiStep);break;
case Stage::SelectEditToggle:if(!pressed_)finish(Exit::Release);else next();break;
case Stage::WriteEditToggleLatch:write(Write::Latch,0);next();break;
case Stage::ReadEditTogglePage:page_=uint8_t(read(Read::UiPage,0));next();break;
case Stage::SelectEditTogglePage:if(page_)finish(Exit::Release);else next();break;
case Stage::ReadEditToggleValue:edit_=uint8_t(read(Read::Edit,0));next();break;
case Stage::ToggleEditValue:edit_^=1;next();break;
case Stage::WriteEditToggleValue:write(Write::Edit,edit_);next();break;
case Stage::SelectEditToggleGate:if(!childNonzero_)finish(Exit::Refresh);else next();break;
case Stage::RefreshEditToggle:finish(Exit::Refresh);break;
case Stage::SelectEditEvent:if(!(event_>>8))finish(Exit::Release);else next();break;
case Stage::WriteEditEventLatch:write(Write::Latch,0);next();break;
case Stage::ReadEditEventPage:page_=uint8_t(read(Read::UiPage,0));next();break;
case Stage::SelectEditEventPage:if(page_)finish(Exit::Release);else next();break;
case Stage::ReadEditEventScreen:screen_=uint8_t(read(Read::Screen,0));next();break;
case Stage::SelectEditEventScreen:stage_=uint8_t(event_)==screen_?Stage::ReadEditEventActive:Stage::BindEditEventOn;break;
case Stage::WriteEditEventOn:edit_=1;write(Write::Edit,edit_);next();break;
case Stage::JoinEditEvent:stage_=Stage::ApplyEditEvent;break;
case Stage::ReadEditEventActive:edit_=uint8_t(read(Read::Edit,0));next();break;
case Stage::ToggleEditEventActive:edit_^=1;next();break;
case Stage::WriteEditEventActive:write(Write::Edit,edit_);next();break;
case Stage::SelectEditEventActive:if(!edit_)finish(Exit::Refresh);else next();break;
case Stage::RefreshEditEvent:finish(Exit::Refresh);break;
case Stage::SelectPatchToggleOne:if(!pressed_)finish(Exit::Release);else next();break;
case Stage::ReadPatchToggleOneBase:patch_=read(Read::PatchBase,0);next();break;
case Stage::ReadPatchToggleOne:value_=uint8_t(read(Read::PatchByte,patch_+215));next();break;
case Stage::FlipPatchToggleOne:value_^=1;next();break;
case Stage::ApplyPatchToggleOne:finish(Exit::ApplyPatch);break;
case Stage::SelectPatchToggleTwo:if(!pressed_)finish(Exit::Release);else next();break;
case Stage::ReadPatchToggleTwoBase:patch_=read(Read::PatchBase,0);next();break;
case Stage::ReadPatchToggleTwo:value_=uint8_t(read(Read::PatchByte,patch_+213));next();break;
case Stage::FlipPatchToggleTwo:value_^=1;next();break;
case Stage::ApplyPatchToggleTwo:finish(Exit::ApplyPatch);break;
case Stage::WriteUiLast:write(Write::Clear167fb,0);next();break;
case Stage::WriteUiSpecial:write(Write::Clear1679e,0);next();break;
case Stage::WriteUiAuxiliary:write(Write::Clear167a0,0);next();break;
case Stage::WriteUiOther:write(Write::Clear1679f,0);next();break;
case Stage::WriteUiEdit:write(Write::Clear16784,0);next();break;
case Stage::WriteUiLatch:write(Write::Clear16792,0);next();break;
case Stage::WriteUiMulti:write(Write::Clear167d5,0);next();break;
case Stage::WriteUiPage:write(Write::Clear16790,0);next();break;
case Stage::WriteUiPart:write(Write::Clear16799,0);next();break;
case Stage::WriteUiTranspose:write(Write::Clear167a3,0);next();break;
case Stage::WriteUiNote:write(Write::Clear16783,0);next();break;
case Stage::ReturnAction:stage_=Stage::Complete;break;
case Stage::ResetSingleUi:await(Stage::RestoreSinglePart);break;
case Stage::RefreshSingleRouting:await(Stage::MarkSingleReload);break;
case Stage::LoadSingleProgram:await(Stage::RefreshSingleScreen);break;
case Stage::RefreshSingleScreen:await(Stage::ResetSingleVoices);break;
case Stage::ResetPlaybackContext:await(Stage::ClearPlaybackEdit);break;
case Stage::ClearPlaybackEdit:await(Stage::RefreshPlaybackScreen);break;
case Stage::RefreshPlaybackScreen:await(Stage::ResetPlaybackVoices);break;
case Stage::ReturnEditClear:stage_=Stage::Complete;break;
case Stage::ResetMultiUi:await(Stage::ReadMultiCurrent);break;
case Stage::RefreshMultiRouting:await(Stage::TestMultiMode);break;
case Stage::LoadMultiProgram:await(Stage::ResolveMultiParts);break;
case Stage::ResolveMultiParts:await(Stage::RefreshResolvedMulti);break;
case Stage::RefreshResolvedMulti:await(Stage::RefreshMultiScreen);break;
case Stage::RefreshMultiScreen:await(Stage::ResetMultiVoices);break;
case Stage::ApplyProgramStep:await(Stage::ReturnProgramStep);break;
case Stage::ReturnProgramStep:stage_=Stage::Complete;break;
case Stage::ResetEditToggleContext:await(Stage::ReadEditToggleValue);break;
case Stage::ObserveEditToggleGate:await(Stage::SelectEditToggleGate);break;
case Stage::ReloadEditCalibration:await(Stage::RefreshEditToggle);break;
case Stage::ResetEditEventContext:await(Stage::ReadEditEventScreen);break;
case Stage::ApplyEditEvent:await(Stage::RefreshEditEvent);break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;void finish(Exit e){exit_=e;stage_=Stage::Complete;}Exit exit_=Exit::None;uint16_t event_=0,preset_=0;uint32_t patch_=0;bool pressed_=true,childNonzero_=false;uint8_t part_=0,mode_=0,active_=0,index_=0,mask_=0,held_=0,delta_=0,page_=0,special_=0,multi_=0,edit_=0,screen_=0,value_=0;
};
}
