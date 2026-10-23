#pragma once
#include <cstdint>
namespace ks {
// StateBrowseWorkflow semantic policy: logical data, decisions and deferred effects; compatibility frame stays in Calls.
class StateBrowseWorkflowTask final {
public:
 enum class Kind { Browse, Repeat, Slow, Fast, Stop, Stopped, Notice, Open, Close, Save, Restore };enum class Stage { ReadBrowseMode, SelectBrowseMode, ReadBrowseMulti, SelectBrowseMulti, SaveProgramEvent, StepProgram, RestoreProgramEvent, TestProgramResult, SelectProgramResult, CompareProgramResult, SelectProgramReload, ReadProgramPage, SelectProgramPage, ArmProgramRepeat, LoadProgramContext, RefreshProgramBrowser, ShowProgramBrowser, ReturnProgramBrowser, ReadBrowsePart, SelectBrowsePart, SaveBankEvent, StepBank, RestoreBankEvent, TestBankResult, SelectBankResult, CompareBankResult, SelectBankDisplay, ReadBankPage, SelectBankDisplayPage, LoadBankContext, RefreshBankBrowser, ShowBankBrowser, ReadBankPartsPage, SelectBankPartsPage, ShowBankParts, ReturnBankBrowser, ReadPartPage, SelectPartPage, SavePartEvent, StepPart, RestorePartEvent, TestPartResult, SelectPartResult, ComparePartResult, SelectPartDisplay, ReadPartDisplayPage, SelectPartReload, PreparePart, LoadPartContext, RefreshPartBrowser, ReadPartShowPage, SelectPartShowPage, ShowPartParts, ReturnPartBrowser, CompareRepeatKey, SelectRepeatKey, WriteRepeatDelta, SelectRepeatDelta, BindRepeatDelay, BindRepeatCallback, ScheduleRepeat, ReturnRepeat, BindSlowWeight, JoinSlowWeight, BindFastWeight, ReadRepeatDelta, MultiplyRepeatDelta, BindRepeatHigh, ClearRepeatLow, StepRepeatedProgram, CompareRepeatedResult, SelectRepeatedResult, LoadRepeatedContext, RefreshRepeatedScreen, BindNextRepeatDelay, BindNextRepeatCallback, ScheduleNextRepeat, ReturnRepeated, BindStopDelay, BindStopCallback, ScheduleStop, ReturnStop, BindBrowseGlobal, ReadBrowseGlobal, ApplyBrowseGlobal, BindBrowseNotice, ShowBrowseNotice, ReadOpenedFlag, SelectOpenedFlag, SaveOriginalProgram, PrepareOpenedProgram, RefreshOpenedPrograms, RefreshOpenedVoices, BindOpenedFlag, WriteOpenedFlag, ReturnOpened, ReadClosedFlag, SelectClosedFlag, RestoreOriginalProgram, ClearOpenedFlag, WriteClosedFlag, ResetClosedMidi, RefreshClosedPrograms, RefreshClosedVoices, RefreshClosedEvents, ReturnClosed, PrepareSavedProgram, ReadSavedProgram, BindSavedBuffer, BindSavedLength, ReadSavedByte, WriteSavedByte, NextSavedDestination, TickSavedLength, RepeatSavedCopy, ReturnSaved, BindRestoreBuffer, ReadRestoreProgram, BindRestoreLength, ReadRestoreByte, WriteRestoreByte, NextRestoreDestination, TickRestoreLength, RepeatRestoreCopy, PublishRestoredProgram, ReturnRestored, AwaitChild, Complete };
 enum class Exit { None, Refresh, Notice };Exit exit()const{return exit_;}enum class Read { Mode, Multi, Page, Part, RepeatDelta, Global, Opened, Patch, Byte };enum class Write { RepeatDelta, Opened, Byte };uint32_t value()const{return value_;}uint32_t source()const{return source_;}uint32_t destination()const{return destination_;}uint32_t delta()const{return delta_;}
 explicit StateBrowseWorkflowTask(Kind kind,uint16_t event=0){key_=uint8_t(event);delta_=uint8_t(event>>8);switch(kind){case Kind::Browse:stage_=Stage::ReadBrowseMode;break;case Kind::Repeat:stage_=Stage::CompareRepeatKey;break;case Kind::Slow:stage_=Stage::BindSlowWeight;break;case Kind::Fast:stage_=Stage::BindFastWeight;break;case Kind::Stop:stage_=Stage::BindStopDelay;break;case Kind::Stopped:stage_=Stage::ReturnStop;break;case Kind::Notice:stage_=Stage::BindBrowseGlobal;break;case Kind::Open:stage_=Stage::ReadOpenedFlag;break;case Kind::Close:stage_=Stage::ReadClosedFlag;break;case Kind::Save:stage_=Stage::PrepareSavedProgram;break;case Kind::Restore:stage_=Stage::BindRestoreBuffer;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint8_t result){if(!awaitingChild())return false;reply_=result;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ReadBrowseMode:case Stage::ReadBrowseMulti:case Stage::ReadProgramPage:case Stage::ReadBrowsePart:case Stage::ReadBankPage:case Stage::ReadBankPartsPage:case Stage::ReadPartPage:case Stage::ReadPartDisplayPage:case Stage::ReadPartShowPage:case Stage::WriteRepeatDelta:case Stage::ReadRepeatDelta:case Stage::ReadOpenedFlag:case Stage::WriteOpenedFlag:case Stage::ReadClosedFlag:case Stage::WriteClosedFlag:return 9+store; case Stage::SaveProgramEvent:case Stage::RestoreProgramEvent:case Stage::SaveBankEvent:case Stage::RestoreBankEvent:case Stage::SavePartEvent:case Stage::RestorePartEvent:return 11; case Stage::StepProgram:case Stage::ArmProgramRepeat:case Stage::LoadProgramContext:case Stage::ShowProgramBrowser:case Stage::StepBank:case Stage::LoadBankContext:case Stage::ShowBankBrowser:case Stage::ShowBankParts:case Stage::StepPart:case Stage::PreparePart:case Stage::LoadPartContext:case Stage::ShowPartParts:case Stage::ScheduleRepeat:case Stage::StepRepeatedProgram:case Stage::LoadRepeatedContext:case Stage::RefreshRepeatedScreen:case Stage::ScheduleNextRepeat:case Stage::ScheduleStop:case Stage::ApplyBrowseGlobal:case Stage::PrepareOpenedProgram:case Stage::RefreshOpenedPrograms:case Stage::RefreshOpenedVoices:case Stage::ResetClosedMidi:case Stage::RefreshClosedPrograms:case Stage::RefreshClosedVoices:case Stage::RefreshClosedEvents:case Stage::PrepareSavedProgram:case Stage::PublishRestoredProgram:return 20; case Stage::RefreshProgramBrowser:case Stage::RefreshBankBrowser:case Stage::RefreshPartBrowser:case Stage::ShowBrowseNotice:return 8; case Stage::ReturnProgramBrowser:case Stage::ReturnBankBrowser:case Stage::ReturnPartBrowser:case Stage::ReturnRepeat:case Stage::ReturnRepeated:case Stage::ReturnStop:case Stage::ReturnOpened:case Stage::ReturnClosed:case Stage::ReturnSaved:case Stage::ReturnRestored:return 17; case Stage::BindRepeatDelay:case Stage::BindNextRepeatDelay:case Stage::BindStopDelay:case Stage::BindSavedLength:case Stage::BindRestoreLength:return 6; case Stage::BindRepeatCallback:case Stage::BindNextRepeatCallback:case Stage::BindStopCallback:case Stage::BindBrowseGlobal:case Stage::BindBrowseNotice:case Stage::BindSavedBuffer:case Stage::BindRestoreBuffer:return 9; case Stage::MultiplyRepeatDelta:return 18; case Stage::ReadBrowseGlobal:case Stage::ReadSavedProgram:case Stage::ReadRestoreProgram:return 12+store; case Stage::SaveOriginalProgram:case Stage::RestoreOriginalProgram:return 15; case Stage::ReadSavedByte:case Stage::ReadRestoreByte:return 5+store; case Stage::WriteSavedByte:case Stage::WriteRestoreByte:return 3+store; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadBrowseMode:value_=uint8_t(read(Read::Mode,0));next();break;
case Stage::SelectBrowseMode:stage_=value_?Stage::ReadBrowseMulti:Stage::SaveProgramEvent;break;
case Stage::ReadBrowseMulti:value_=uint8_t(read(Read::Multi,0));next();break;
case Stage::SelectBrowseMulti:stage_=value_?Stage::SaveProgramEvent:Stage::ReadBrowsePart;break;
case Stage::SelectProgramResult:stage_=reply_?Stage::CompareProgramResult:Stage::ReturnProgramBrowser;break;
case Stage::SelectProgramReload:stage_=reply_==1?Stage::ShowProgramBrowser:Stage::ReadProgramPage;break;
case Stage::ReadProgramPage:value_=uint8_t(read(Read::Page,0));next();break;
case Stage::SelectProgramPage:stage_=value_?Stage::ShowProgramBrowser:Stage::ArmProgramRepeat;break;
case Stage::ReadBrowsePart:value_=uint8_t(read(Read::Part,0));next();break;
case Stage::SelectBrowsePart:stage_=value_?Stage::ReadPartPage:Stage::SaveBankEvent;break;
case Stage::SelectBankResult:stage_=reply_?Stage::CompareBankResult:Stage::ReturnBankBrowser;break;
case Stage::SelectBankDisplay:stage_=reply_==1?Stage::ShowBankBrowser:Stage::ReadBankPage;break;
case Stage::ReadBankPage:value_=uint8_t(read(Read::Page,0));next();break;
case Stage::SelectBankDisplayPage:stage_=value_?Stage::ShowBankBrowser:Stage::LoadBankContext;break;
case Stage::ReadBankPartsPage:value_=uint8_t(read(Read::Page,0));next();break;
case Stage::SelectBankPartsPage:stage_=value_?Stage::ReturnBankBrowser:Stage::ShowBankParts;break;
case Stage::ReadPartPage:value_=uint8_t(read(Read::Page,0));next();break;
case Stage::SelectPartPage:stage_=value_?Stage::SaveProgramEvent:Stage::SavePartEvent;break;
case Stage::SelectPartResult:stage_=reply_?Stage::ComparePartResult:Stage::ReturnPartBrowser;break;
case Stage::SelectPartDisplay:stage_=reply_==1?Stage::ReadPartShowPage:Stage::ReadPartDisplayPage;break;
case Stage::ReadPartDisplayPage:value_=uint8_t(read(Read::Page,0));next();break;
case Stage::SelectPartReload:stage_=value_?Stage::ReadPartShowPage:Stage::PreparePart;break;
case Stage::ReadPartShowPage:value_=uint8_t(read(Read::Page,0));next();break;
case Stage::SelectPartShowPage:stage_=value_?Stage::ReturnPartBrowser:Stage::ShowPartParts;break;
case Stage::SelectRepeatKey:stage_=key_==240?Stage::ReturnRepeat:Stage::WriteRepeatDelta;break;
case Stage::WriteRepeatDelta:write(Write::RepeatDelta,0,delta_);next();break;
case Stage::SelectRepeatDelta:stage_=delta_?Stage::BindRepeatDelay:Stage::ReturnRepeat;break;
case Stage::BindSlowWeight:weight_=10;next();break;
case Stage::BindFastWeight:weight_=9;next();break;
case Stage::ReadRepeatDelta:delta_=uint8_t(read(Read::RepeatDelta,0));next();break;
case Stage::MultiplyRepeatDelta:delta_=uint8_t(int(int8_t(delta_))*weight_);next();break;
case Stage::SelectRepeatedResult:stage_=reply_==3?Stage::LoadRepeatedContext:Stage::ReturnRepeated;break;
case Stage::ReadBrowseGlobal:value_=uint8_t(read(Read::Global,0));next();break;
case Stage::ReadOpenedFlag:value_=uint8_t(read(Read::Opened,0));next();break;
case Stage::SelectOpenedFlag:stage_=value_?Stage::ReturnOpened:Stage::SaveOriginalProgram;break;
case Stage::WriteOpenedFlag:write(Write::Opened,0,1);next();break;
case Stage::ReadClosedFlag:value_=uint8_t(read(Read::Opened,0));next();break;
case Stage::SelectClosedFlag:stage_=value_?Stage::RestoreOriginalProgram:Stage::ReturnClosed;break;
case Stage::WriteClosedFlag:write(Write::Opened,0,0);next();break;
case Stage::ReadSavedProgram:source_=read(Read::Patch,0);next();break;
case Stage::BindSavedBuffer:destination_=0x167fc;next();break;
case Stage::BindSavedLength:remaining_=256;next();break;
case Stage::ReadSavedByte:value_=uint8_t(read(Read::Byte,source_++));next();break;
case Stage::WriteSavedByte:write(Write::Byte,destination_,value_);next();break;
case Stage::NextSavedDestination:++destination_;next();break;
case Stage::TickSavedLength:--remaining_;next();break;
case Stage::RepeatSavedCopy:stage_=remaining_?Stage::ReadSavedByte:Stage::ReturnSaved;break;
case Stage::BindRestoreBuffer:source_=0x167fc;next();break;
case Stage::ReadRestoreProgram:destination_=read(Read::Patch,0);next();break;
case Stage::BindRestoreLength:remaining_=256;next();break;
case Stage::ReadRestoreByte:value_=uint8_t(read(Read::Byte,source_++));next();break;
case Stage::WriteRestoreByte:write(Write::Byte,destination_,value_);next();break;
case Stage::NextRestoreDestination:++destination_;next();break;
case Stage::TickRestoreLength:--remaining_;next();break;
case Stage::RepeatRestoreCopy:stage_=remaining_?Stage::ReadRestoreByte:Stage::PublishRestoredProgram;break;
case Stage::RefreshProgramBrowser:finish(Exit::Refresh);break;
case Stage::RefreshBankBrowser:finish(Exit::Refresh);break;
case Stage::RefreshPartBrowser:finish(Exit::Refresh);break;
case Stage::JoinSlowWeight:stage_=Stage::ReadRepeatDelta;break;
case Stage::ShowBrowseNotice:finish(Exit::Notice);break;
case Stage::StepProgram:await(Stage::RestoreProgramEvent);break;
case Stage::ArmProgramRepeat:await(Stage::LoadProgramContext);break;
case Stage::LoadProgramContext:await(Stage::RefreshProgramBrowser);break;
case Stage::ShowProgramBrowser:await(Stage::ReturnProgramBrowser);break;
case Stage::ReturnProgramBrowser:stage_=Stage::Complete;break;
case Stage::StepBank:await(Stage::RestoreBankEvent);break;
case Stage::LoadBankContext:await(Stage::RefreshBankBrowser);break;
case Stage::ShowBankBrowser:await(Stage::ReadBankPartsPage);break;
case Stage::ShowBankParts:await(Stage::ReturnBankBrowser);break;
case Stage::ReturnBankBrowser:stage_=Stage::Complete;break;
case Stage::StepPart:await(Stage::RestorePartEvent);break;
case Stage::PreparePart:await(Stage::LoadPartContext);break;
case Stage::LoadPartContext:await(Stage::RefreshPartBrowser);break;
case Stage::ShowPartParts:await(Stage::ReturnPartBrowser);break;
case Stage::ReturnPartBrowser:stage_=Stage::Complete;break;
case Stage::ScheduleRepeat:await(Stage::ReturnRepeat);break;
case Stage::ReturnRepeat:stage_=Stage::Complete;break;
case Stage::StepRepeatedProgram:await(Stage::CompareRepeatedResult);break;
case Stage::LoadRepeatedContext:await(Stage::RefreshRepeatedScreen);break;
case Stage::RefreshRepeatedScreen:await(Stage::BindNextRepeatDelay);break;
case Stage::ScheduleNextRepeat:await(Stage::ReturnRepeated);break;
case Stage::ReturnRepeated:stage_=Stage::Complete;break;
case Stage::ScheduleStop:await(Stage::ReturnStop);break;
case Stage::ReturnStop:stage_=Stage::Complete;break;
case Stage::ApplyBrowseGlobal:await(Stage::BindBrowseNotice);break;
case Stage::SaveOriginalProgram:await(Stage::PrepareOpenedProgram);break;
case Stage::PrepareOpenedProgram:await(Stage::RefreshOpenedPrograms);break;
case Stage::RefreshOpenedPrograms:await(Stage::RefreshOpenedVoices);break;
case Stage::RefreshOpenedVoices:await(Stage::BindOpenedFlag);break;
case Stage::ReturnOpened:stage_=Stage::Complete;break;
case Stage::RestoreOriginalProgram:await(Stage::ClearOpenedFlag);break;
case Stage::ResetClosedMidi:await(Stage::RefreshClosedPrograms);break;
case Stage::RefreshClosedPrograms:await(Stage::RefreshClosedVoices);break;
case Stage::RefreshClosedVoices:await(Stage::RefreshClosedEvents);break;
case Stage::RefreshClosedEvents:await(Stage::ReturnClosed);break;
case Stage::ReturnClosed:stage_=Stage::Complete;break;
case Stage::PrepareSavedProgram:await(Stage::ReadSavedProgram);break;
case Stage::ReturnSaved:stage_=Stage::Complete;break;
case Stage::PublishRestoredProgram:await(Stage::ReturnRestored);break;
case Stage::ReturnRestored:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;void finish(Exit e){exit_=e;stage_=Stage::Complete;}Exit exit_=Exit::None;uint32_t source_=0,destination_=0;uint16_t remaining_=0;uint8_t value_=0,reply_=0,key_=0,delta_=0,weight_=0;
};
}
