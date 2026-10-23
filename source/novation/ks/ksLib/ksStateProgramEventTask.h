#pragma once
#include <cstdint>
namespace ks {
// StateProgramEvent semantic policy: logical data, decisions and deferred effects; compatibility frame stays in Calls.
class StateProgramEventTask final {
public:
 enum class Kind { Event };enum class Stage { TestMultiEvent, SelectMultiEvent, ResolveEventTarget, ReadEventBank, TestEventSign, SelectEventSign, TestSingleEvent, SelectSingleEvent, MaskSingleBank, ReadPreviousMode, ClearSingleMode, WriteSingleMode, ComparePreviousMode, SelectModeReset, SaveSingleEvent, ResetSingleVoices, ResetSingleUi, RestoreSingleEvent, ClearActiveValue, WriteActiveValue, RefreshSingleRouting, BindSingleTarget, BindEventBank, SelectEmptyEvent, BindProgramLimit, CompareProgramBank, SelectProgramLimit, CompareAlternateBank, RejectAlternateBank, BindAlternateLimit, CompareProgramIndex, RejectProgramIndex, WriteTargetBank, WriteTargetIndex, WriteCurrentBank, WriteCurrentIndex, NotifyProgramChange, ReadSavedPart, SaveSavedPart, ReadActivePart, WriteActivePart, LoadProgram, RestoreSavedPart, WriteSavedPart, BindReloadFlag, WriteReloadFlag, RefreshRouting, RefreshScreen, ReturnProgramEvent, AwaitChild, Complete };
 enum class Exit { None, Multi };Exit exit()const{return exit_;}enum class Read { Target, Mode, Part, Active };enum class Write { Target, Mode, Active, CurrentBank, CurrentIndex, Part, Reload };uint32_t previousBank()const{return previousBank_;}uint32_t mode()const{return mode_;}uint32_t savedPart()const{return savedPart_;}uint32_t activePart()const{return activePart_;}
 explicit StateProgramEventTask(Kind kind,uint16_t event=0){bank_=uint8_t(event>>8);index_=uint8_t(event);switch(kind){case Kind::Event:stage_=Stage::TestMultiEvent;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint32_t pointer){if(!awaitingChild())return false;if(resume_==Stage::ReadEventBank)pointer_=pointer;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::SelectMultiEvent:return 8; case Stage::ResolveEventTarget:case Stage::ResetSingleVoices:case Stage::ResetSingleUi:case Stage::RefreshSingleRouting:case Stage::LoadProgram:case Stage::RefreshRouting:case Stage::RefreshScreen:return 20; case Stage::ReadEventBank:case Stage::WriteTargetBank:return 3+store; case Stage::ReadPreviousMode:case Stage::WriteSingleMode:case Stage::WriteActiveValue:case Stage::WriteCurrentBank:case Stage::WriteCurrentIndex:case Stage::ReadSavedPart:case Stage::ReadActivePart:case Stage::WriteActivePart:case Stage::WriteSavedPart:case Stage::WriteReloadFlag:return 9+store; case Stage::SaveSingleEvent:case Stage::RestoreSingleEvent:case Stage::SaveSavedPart:case Stage::RestoreSavedPart:return 11; case Stage::BindSingleTarget:return 9; case Stage::WriteTargetIndex:return 6+store; case Stage::NotifyProgramChange:return 15; case Stage::ReturnProgramEvent:return 17; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::SelectMultiEvent:if(bank_&64)finish(Exit::Multi);else next();break;
case Stage::ReadEventBank:previousBank_=uint8_t(read(Read::Target,pointer_));next();break;
case Stage::SelectEventSign:stage_=bank_&128?Stage::BindProgramLimit:Stage::TestSingleEvent;break;
case Stage::SelectSingleEvent:stage_=bank_&32?Stage::MaskSingleBank:Stage::BindEventBank;break;
case Stage::MaskSingleBank:bank_&=31;next();break;
case Stage::ReadPreviousMode:mode_=uint8_t(read(Read::Mode,0));next();break;
case Stage::WriteSingleMode:write(Write::Mode,0,0);next();break;
case Stage::SelectModeReset:stage_=mode_?Stage::SaveSingleEvent:Stage::ClearActiveValue;break;
case Stage::WriteActiveValue:write(Write::Active,0,0);next();break;
case Stage::BindSingleTarget:pointer_=0x167ae;next();break;
case Stage::BindEventBank:previousBank_=bank_;next();break;
case Stage::SelectEmptyEvent:stage_=previousBank_?Stage::BindProgramLimit:Stage::ReturnProgramEvent;break;
case Stage::BindProgramLimit:limit_=100;next();break;
case Stage::SelectProgramLimit:stage_=previousBank_<=4?Stage::CompareProgramIndex:Stage::CompareAlternateBank;break;
case Stage::RejectAlternateBank:stage_=previousBank_>8?Stage::ReturnProgramEvent:Stage::BindAlternateLimit;break;
case Stage::BindAlternateLimit:limit_=50;next();break;
case Stage::RejectProgramIndex:stage_=index_>=limit_?Stage::ReturnProgramEvent:Stage::WriteTargetBank;break;
case Stage::WriteTargetBank:write(Write::Target,pointer_,previousBank_);next();break;
case Stage::WriteTargetIndex:write(Write::Target,pointer_+1,index_);next();break;
case Stage::WriteCurrentBank:write(Write::CurrentBank,0,previousBank_);next();break;
case Stage::WriteCurrentIndex:write(Write::CurrentIndex,0,index_);next();break;
case Stage::ReadSavedPart:savedPart_=uint8_t(read(Read::Part,0));next();break;
case Stage::ReadActivePart:activePart_=uint8_t(read(Read::Active,0));next();break;
case Stage::WriteActivePart:write(Write::Part,0,activePart_);next();break;
case Stage::WriteSavedPart:write(Write::Part,0,savedPart_);next();break;
case Stage::WriteReloadFlag:write(Write::Reload,0,255);next();break;
case Stage::ResolveEventTarget:await(Stage::ReadEventBank);break;
case Stage::ResetSingleVoices:await(Stage::ResetSingleUi);break;
case Stage::ResetSingleUi:await(Stage::RestoreSingleEvent);break;
case Stage::RefreshSingleRouting:await(Stage::BindSingleTarget);break;
case Stage::NotifyProgramChange:await(Stage::ReadSavedPart);break;
case Stage::LoadProgram:await(Stage::RestoreSavedPart);break;
case Stage::RefreshRouting:await(Stage::RefreshScreen);break;
case Stage::RefreshScreen:await(Stage::ReturnProgramEvent);break;
case Stage::ReturnProgramEvent:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;void finish(Exit e){exit_=e;stage_=Stage::Complete;}Exit exit_=Exit::None;uint32_t pointer_=0;uint8_t bank_=0,index_=0,previousBank_=0,mode_=0,limit_=0,savedPart_=0,activePart_=0;
};
}
