#pragma once
#include <cstdint>
namespace ks {
// Menu action routing, repeat navigation, toggle and Store policy. Handler addresses and callback continuations belong only to the ABI bridge.
class StateMenuActionsTask final {
public:
 enum class Kind { Toggle, Auxiliary, Dispatch, Previous, RepeatPrevious, Next, RepeatNext, Store };enum class Stage { SelectToggle, BindToggle, FlipToggle, RefreshToggle, SelectAuxiliary, EnterAuxiliary, ClearActionIndex, ReadActionIndex, ScaleActionIndex, BindActionTable, ReadActionHandler, TestActionEvent, DispatchAction, SelectPrevious, ReadPreviousPage, TestPreviousPage, RoutePreviousName, ReadPreviousSpecial, RoutePreviousSpecial, ObservePreviousEdit, SelectPreviousEdit, BindPreviousDelta, EditPrevious, BindPreviousDelay, JoinPreviousRepeat, BindPreviousRepeat, BindPreviousCallback, SchedulePrevious, PreviousProgram, SelectNext, ReadNextPage, TestNextPage, RouteNextName, ReadNextSpecial, RouteNextSpecial, ObserveNextEdit, SelectNextEdit, BindNextDelta, EditNext, BindNextDelay, JoinNextRepeat, BindNextRepeat, BindNextCallback, ScheduleNext, NextProgram, SelectStore, ClearStoreFlag, ClearStoreSpecial, ClearStoreAuxiliary, ObserveCalibrationGate, SelectCalibrationGate, SaveCalibration, BindCalibrationNotice, ShowCalibrationNotice, FinishCalibration, ReadStoreEdit, SelectStoreEdit, ReadStoreScreen, TestProgramScreen, SelectProgramScreen, StoreProgram, BindProgramNotice, ShowProgramNotice, FinishProgram, TestStoreScreen, SelectStoreScreen, ReadStorePage, SelectFirstStorePage, DecrementStorePage, SelectSecondStorePage, ReturnStore, ReadStoreUi, SelectProgramStore, TestMultiStore, SelectMultiStore, TestPanelStore, SelectPanelStore, ReadStoreMode, SelectStoreMode, ReadStorePart, SelectStorePart, AwaitChild, Complete };
 enum class Read { Toggle, UiPage, Special, Edit, Screen, Page, Mode, Part, Handler };enum class Write { Toggle, Special, Auxiliary };enum class Exit { None, Release, RefreshToggle, Auxiliary, ReleaseNavigation, PreviousName, PreviousSpecial, NextName, NextSpecial, Edit, PreviousProgram, NextProgram, Notice, FirstPage, SecondPage, ProgramStore, MultiStore, PanelStore, PartStore, CommonStore, Handler };Exit exit()const{return exit_;}uint8_t index()const{return index_;}uint8_t toggle()const{return toggle_;}uint8_t page()const{return page_;}uint8_t special()const{return special_;}uint8_t edit()const{return edit_;}uint8_t screen()const{return screen_;}uint8_t ui()const{return ui_;}uint8_t mode()const{return mode_;}uint8_t part()const{return part_;}
 explicit StateMenuActionsTask(Kind kind,uint16_t event=0,bool pressed=true){event_=event;pressed_=pressed;switch(kind){case Kind::Toggle:stage_=Stage::SelectToggle;break;case Kind::Auxiliary:stage_=Stage::SelectAuxiliary;break;case Kind::Dispatch:stage_=Stage::ClearActionIndex;break;case Kind::Previous:stage_=Stage::SelectPrevious;break;case Kind::RepeatPrevious:stage_=Stage::BindPreviousRepeat;break;case Kind::Next:stage_=Stage::SelectNext;break;case Kind::RepeatNext:stage_=Stage::BindNextRepeat;break;case Kind::Store:stage_=Stage::SelectStore;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(bool nonzero=false){if(!awaitingChild())return false;childNonzero_=nonzero;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::SelectToggle:case Stage::RefreshToggle:case Stage::SelectAuxiliary:case Stage::EnterAuxiliary:case Stage::SelectPrevious:case Stage::RoutePreviousName:case Stage::RoutePreviousSpecial:case Stage::EditPrevious:case Stage::PreviousProgram:case Stage::SelectNext:case Stage::RouteNextName:case Stage::RouteNextSpecial:case Stage::EditNext:case Stage::NextProgram:case Stage::SelectStore:case Stage::FinishCalibration:case Stage::FinishProgram:case Stage::SelectFirstStorePage:case Stage::SelectSecondStorePage:case Stage::SelectProgramStore:case Stage::SelectMultiStore:case Stage::SelectStoreMode:case Stage::SelectStorePart:return 8; case Stage::BindToggle:case Stage::BindActionTable:case Stage::BindPreviousCallback:case Stage::BindNextCallback:case Stage::BindCalibrationNotice:case Stage::BindProgramNotice:return 9; case Stage::FlipToggle:case Stage::ClearStoreSpecial:case Stage::ClearStoreAuxiliary:return 9+store; case Stage::ReadActionHandler:case Stage::ReadPreviousPage:case Stage::ReadPreviousSpecial:case Stage::ReadNextPage:case Stage::ReadNextSpecial:case Stage::ReadStoreEdit:case Stage::ReadStoreScreen:case Stage::ReadStorePage:case Stage::ReadStoreUi:case Stage::ReadStoreMode:case Stage::ReadStorePart:return 12; case Stage::ObservePreviousEdit:case Stage::SchedulePrevious:case Stage::ObserveNextEdit:case Stage::ScheduleNext:case Stage::ObserveCalibrationGate:case Stage::SaveCalibration:case Stage::ShowCalibrationNotice:case Stage::StoreProgram:case Stage::ShowProgramNotice:return 20; case Stage::BindPreviousDelta:case Stage::BindPreviousDelay:case Stage::BindPreviousRepeat:case Stage::BindNextDelta:case Stage::BindNextDelay:case Stage::BindNextRepeat:return 6; case Stage::ReturnStore:return 17; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::SelectToggle:if(!pressed_)finish(Exit::Release);else next();break;
case Stage::FlipToggle:toggle_=uint8_t(read(Read::Toggle))^1;write(Write::Toggle,toggle_);next();break;
case Stage::RefreshToggle:finish(Exit::RefreshToggle);break;
case Stage::SelectAuxiliary:if(!pressed_)finish(Exit::Release);else next();break;
case Stage::EnterAuxiliary:finish(Exit::Auxiliary);break;
case Stage::ReadActionIndex:index_=uint8_t(event_);next();break;
case Stage::ScaleActionIndex:index_=uint8_t(index_<<2);next();break;
case Stage::ReadActionHandler:read(Read::Handler);next();break;
case Stage::DispatchAction:finish(Exit::Handler);break;
case Stage::SelectPrevious:if(!pressed_)finish(Exit::ReleaseNavigation);else next();break;
case Stage::ReadPreviousPage:page_=uint8_t(read(Read::UiPage));next();break;
case Stage::RoutePreviousName:if(page_==2)finish(Exit::PreviousName);else next();break;
case Stage::ReadPreviousSpecial:special_=uint8_t(read(Read::Special));next();break;
case Stage::RoutePreviousSpecial:if(special_)finish(Exit::PreviousSpecial);else next();break;
case Stage::SelectPreviousEdit:stage_=childNonzero_?Stage::BindPreviousDelay:Stage::BindPreviousDelta;break;
case Stage::EditPrevious:finish(Exit::Edit);break;
case Stage::JoinPreviousRepeat:stage_=Stage::BindPreviousCallback;break;
case Stage::PreviousProgram:finish(Exit::PreviousProgram);break;
case Stage::SelectNext:if(!pressed_)finish(Exit::ReleaseNavigation);else next();break;
case Stage::ReadNextPage:page_=uint8_t(read(Read::UiPage));next();break;
case Stage::RouteNextName:if(page_==2)finish(Exit::NextName);else next();break;
case Stage::ReadNextSpecial:special_=uint8_t(read(Read::Special));next();break;
case Stage::RouteNextSpecial:if(special_)finish(Exit::NextSpecial);else next();break;
case Stage::SelectNextEdit:stage_=childNonzero_?Stage::BindNextDelay:Stage::BindNextDelta;break;
case Stage::EditNext:finish(Exit::Edit);break;
case Stage::JoinNextRepeat:stage_=Stage::BindNextCallback;break;
case Stage::NextProgram:finish(Exit::NextProgram);break;
case Stage::SelectStore:if(!pressed_)finish(Exit::Release);else next();break;
case Stage::ClearStoreSpecial:write(Write::Special,0);next();break;
case Stage::ClearStoreAuxiliary:write(Write::Auxiliary,0);next();break;
case Stage::SelectCalibrationGate:stage_=childNonzero_?Stage::SaveCalibration:Stage::ReadStoreEdit;break;
case Stage::FinishCalibration:finish(Exit::Notice);break;
case Stage::ReadStoreEdit:edit_=uint8_t(read(Read::Edit));next();break;
case Stage::SelectStoreEdit:stage_=edit_?Stage::ReadStoreScreen:Stage::ReadStoreUi;break;
case Stage::ReadStoreScreen:screen_=uint8_t(read(Read::Screen));next();break;
case Stage::SelectProgramScreen:stage_=screen_?Stage::TestStoreScreen:Stage::StoreProgram;break;
case Stage::FinishProgram:finish(Exit::Notice);break;
case Stage::SelectStoreScreen:stage_=screen_==9?Stage::ReadStorePage:Stage::ReadStoreUi;break;
case Stage::ReadStorePage:page_=uint8_t(read(Read::Page));next();break;
case Stage::SelectFirstStorePage:if(!page_)finish(Exit::FirstPage);else next();break;
case Stage::DecrementStorePage:page_=uint8_t(page_-1);next();break;
case Stage::SelectSecondStorePage:if(!page_)finish(Exit::SecondPage);else next();break;
case Stage::ReadStoreUi:ui_=uint8_t(read(Read::UiPage));next();break;
case Stage::SelectProgramStore:if(!ui_)finish(Exit::ProgramStore);else next();break;
case Stage::SelectMultiStore:if(ui_==1)finish(Exit::MultiStore);else next();break;
case Stage::SelectPanelStore:if(ui_==3)finish(Exit::PanelStore);else next();break;
case Stage::ReadStoreMode:mode_=uint8_t(read(Read::Mode));next();break;
case Stage::SelectStoreMode:if(!mode_)finish(Exit::PartStore);else next();break;
case Stage::ReadStorePart:part_=uint8_t(read(Read::Part));next();break;
case Stage::SelectStorePart:finish(part_?Exit::PartStore:Exit::CommonStore);break;
case Stage::ObservePreviousEdit:await(Stage::SelectPreviousEdit);break;
case Stage::SchedulePrevious:await(Stage::PreviousProgram);break;
case Stage::ObserveNextEdit:await(Stage::SelectNextEdit);break;
case Stage::ScheduleNext:await(Stage::NextProgram);break;
case Stage::ObserveCalibrationGate:await(Stage::SelectCalibrationGate);break;
case Stage::SaveCalibration:await(Stage::BindCalibrationNotice);break;
case Stage::ShowCalibrationNotice:await(Stage::FinishCalibration);break;
case Stage::StoreProgram:await(Stage::BindProgramNotice);break;
case Stage::ShowProgramNotice:await(Stage::FinishProgram);break;
case Stage::ReturnStore:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;void finish(Exit value){exit_=value;stage_=Stage::Complete;}Exit exit_=Exit::None;uint16_t event_=0;bool pressed_=true,childNonzero_=false;uint8_t index_=0,toggle_=0,page_=0,special_=0,edit_=0,screen_=0,ui_=0,mode_=0,part_=0;
};
}
