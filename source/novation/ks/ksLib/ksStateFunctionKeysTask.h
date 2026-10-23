#pragma once
#include <cstdint>
namespace ks {
// Function key press/release masks, calibration chords, diagnostic dump and edit refresh routing with live post-child observations.
class StateFunctionKeysTask final {
public:
 enum class Kind { Event, Refresh, Clear, Release };enum class Stage { TestFunctionEvent, SelectFunctionEvent, ReadFunctionPage, TestFunctionPage, SelectFunctionPage, ReadFunctionOther, SelectFunctionOther, TestFunctionOther, SelectFunctionOtherName, ReturnFunctionOther, TestFunctionKey, SelectFunctionKey, TestFunctionBit, SelectFunctionBit, SetFunctionBit, TestFunctionZero, SelectFunctionZero, SetFunctionZero, ObserveFunctionEdit, SelectFunctionEdit, ObserveFunctionCalibration, SelectFunctionCalibration, SaveCalibrationKey, ReadCalibrationChord, MaskCalibrationChord, BindCalibrationFirst, TestCalibrationFirst, SelectCalibrationFirst, BindCalibrationSecond, TestCalibrationSecond, SelectCalibrationSecond, RestoreCalibrationKey, JoinCalibrationKey, SelectCalibrationPanel, ReadCalibrationPatch, ReadCalibrationTranspose, ConvertCalibrationTranspose, ReadGeneralChord, MaskGeneralChord, TestGeneralChord, SelectGeneralChord, ClearGeneralChord, BindGeneralSpecial, WriteGeneralSpecial, EnterGeneralSpecial, ReturnGeneralSpecial, ReadDumpChord, MaskDumpChord, TestDumpChord, SelectDumpChord, ClearDumpChord, DumpFirmware, RefreshDump, ObserveFunctionRefresh, SelectFunctionRefresh, BindFunctionReset, ApplyFunctionReset, ReturnFunction, BindFunctionClear, WriteFunctionClear, WriteFunctionEdit, WriteFunctionPage, ReturnFunctionClear, TestReleaseKey, SelectReleaseKey, TestReleaseBit, SelectReleaseBit, ClearReleaseBit, TestReleaseZero, SelectReleaseZero, ClearReleaseZero, ReturnRelease, AwaitChild, Complete };
 enum class Read { UiPage, Other, Patch, Flags, Byte };enum class Write { RawFlags, Flags, Edit, UiPage, Special };enum class Exit { None, Name, Semitone, Special, Refresh, Screen };Exit exit()const{return exit_;}uint32_t page()const{return page_;}uint32_t other()const{return other_;}uint32_t flags()const{return flags_;}uint32_t patch()const{return patch_;}uint32_t transpose()const{return transpose_;}uint32_t record()const{return record_;}
 explicit StateFunctionKeysTask(Kind kind,uint16_t event=0,uint32_t record=0){event_=event;key_=uint8_t(event);record_=record;switch(kind){case Kind::Event:stage_=Stage::TestFunctionEvent;break;case Kind::Refresh:stage_=Stage::ObserveFunctionRefresh;break;case Kind::Clear:stage_=Stage::BindFunctionClear;break;case Kind::Release:stage_=Stage::TestReleaseKey;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(bool nonzero){if(!awaitingChild())return false;childNonzero_=nonzero;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::SelectFunctionEvent:case Stage::SelectFunctionPage:case Stage::SelectFunctionOtherName:case Stage::SelectFunctionKey:case Stage::ConvertCalibrationTranspose:case Stage::EnterGeneralSpecial:case Stage::RefreshDump:case Stage::SelectFunctionRefresh:return 8; case Stage::ReadFunctionPage:case Stage::ReadFunctionOther:return 12; case Stage::ReturnFunctionOther:case Stage::ReturnGeneralSpecial:case Stage::ReturnFunction:case Stage::ReturnFunctionClear:case Stage::ReturnRelease:return 17; case Stage::SetFunctionBit:case Stage::SetFunctionZero:case Stage::WriteGeneralSpecial:case Stage::WriteFunctionEdit:case Stage::WriteFunctionPage:case Stage::ClearReleaseBit:case Stage::ClearReleaseZero:return 9+store; case Stage::ObserveFunctionEdit:case Stage::ObserveFunctionCalibration:case Stage::SelectCalibrationPanel:case Stage::DumpFirmware:case Stage::ObserveFunctionRefresh:case Stage::ApplyFunctionReset:return 20; case Stage::ReadCalibrationChord:case Stage::ReadGeneralChord:case Stage::ReadDumpChord:return 6; case Stage::ReadCalibrationPatch:return 18; case Stage::ReadCalibrationTranspose:case Stage::ClearGeneralChord:case Stage::ClearDumpChord:return 15; case Stage::WriteFunctionClear:return 3+store; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::SelectFunctionEvent:stage_=event_>>8?Stage::ReadFunctionPage:Stage::TestReleaseKey;break;
case Stage::ReadFunctionPage:page_=uint8_t(read(Read::UiPage,0));next();break;
case Stage::SelectFunctionPage:if(page_==2)finish(Exit::Name);else next();break;
case Stage::ReadFunctionOther:other_=uint8_t(read(Read::Other,0));next();break;
case Stage::SelectFunctionOther:stage_=other_?Stage::TestFunctionOther:Stage::TestFunctionKey;break;
case Stage::SelectFunctionOtherName:if(other_==2)finish(Exit::Name);else next();break;
case Stage::SelectFunctionKey:stage_=key_?Stage::TestFunctionBit:Stage::ObserveFunctionRefresh;break;
case Stage::SelectFunctionBit:stage_=key_<=5?Stage::SetFunctionBit:Stage::TestFunctionZero;break;
case Stage::SetFunctionBit:flags_=uint8_t(read(Read::Flags,record_))|uint8_t(1u<<(key_&7));write(Write::RawFlags,record_,flags_);next();break;
case Stage::SelectFunctionZero:stage_=key_==9?Stage::SetFunctionZero:Stage::ObserveFunctionEdit;break;
case Stage::SetFunctionZero:flags_=uint8_t(read(Read::Flags,record_))|1;write(Write::RawFlags,record_,flags_);next();break;
case Stage::SelectFunctionEdit:stage_=childNonzero_?Stage::ObserveFunctionCalibration:Stage::ObserveFunctionRefresh;break;
case Stage::SelectFunctionCalibration:stage_=childNonzero_?Stage::SaveCalibrationKey:Stage::ReadGeneralChord;break;
case Stage::ReadCalibrationChord:flags_=uint8_t(read(Read::Flags,record_));next();break;
case Stage::MaskCalibrationChord:chord_=flags_&0x31;next();break;
case Stage::SelectCalibrationFirst:stage_=chord_==0x11?Stage::SelectCalibrationPanel:Stage::BindCalibrationSecond;break;
case Stage::SelectCalibrationSecond:stage_=chord_==0x21?Stage::SelectCalibrationPanel:Stage::RestoreCalibrationKey;break;
case Stage::JoinCalibrationKey:stage_=Stage::ReadGeneralChord;break;
case Stage::ReadCalibrationPatch:patch_=read(Read::Patch,0);next();break;
case Stage::ReadCalibrationTranspose:transpose_=uint8_t(read(Read::Byte,patch_+227));next();break;
case Stage::ConvertCalibrationTranspose:finish(Exit::Semitone);break;
case Stage::ReadGeneralChord:flags_=uint8_t(read(Read::Flags,record_));next();break;
case Stage::MaskGeneralChord:chord_=flags_&6;next();break;
case Stage::SelectGeneralChord:stage_=chord_==6?Stage::ClearGeneralChord:Stage::ReadDumpChord;break;
case Stage::WriteGeneralSpecial:write(Write::Special,0,0);next();break;
case Stage::EnterGeneralSpecial:finish(Exit::Special);break;
case Stage::ReadDumpChord:flags_=uint8_t(read(Read::Flags,record_));next();break;
case Stage::MaskDumpChord:chord_=flags_&0x38;next();break;
case Stage::SelectDumpChord:stage_=chord_==0x38?Stage::ClearDumpChord:Stage::ObserveFunctionRefresh;break;
case Stage::RefreshDump:finish(Exit::Refresh);break;
case Stage::SelectFunctionRefresh:if(childNonzero_)finish(Exit::Screen);else next();break;
case Stage::WriteFunctionClear:write(Write::Flags,record_,0);next();break;
case Stage::WriteFunctionEdit:write(Write::Edit,0,0);next();break;
case Stage::WriteFunctionPage:write(Write::UiPage,0,0);next();break;
case Stage::SelectReleaseKey:stage_=key_?Stage::TestReleaseBit:Stage::ReturnRelease;break;
case Stage::SelectReleaseBit:stage_=key_<=5?Stage::ClearReleaseBit:Stage::TestReleaseZero;break;
case Stage::ClearReleaseBit:flags_=uint8_t(read(Read::Flags,record_))&uint8_t(~(1u<<(key_&7)));write(Write::RawFlags,record_,flags_);next();break;
case Stage::SelectReleaseZero:stage_=key_==9?Stage::ClearReleaseZero:Stage::ReturnRelease;break;
case Stage::ClearReleaseZero:flags_=uint8_t(read(Read::Flags,record_))&uint8_t(~1u);write(Write::RawFlags,record_,flags_);next();break;
case Stage::ReturnFunctionOther:stage_=Stage::Complete;break;
case Stage::ObserveFunctionEdit:await(Stage::SelectFunctionEdit);break;
case Stage::ObserveFunctionCalibration:await(Stage::SelectFunctionCalibration);break;
case Stage::SelectCalibrationPanel:await(Stage::ReadCalibrationPatch);break;
case Stage::ClearGeneralChord:await(Stage::BindGeneralSpecial);break;
case Stage::ReturnGeneralSpecial:stage_=Stage::Complete;break;
case Stage::ClearDumpChord:await(Stage::DumpFirmware);break;
case Stage::DumpFirmware:await(Stage::RefreshDump);break;
case Stage::ObserveFunctionRefresh:await(Stage::SelectFunctionRefresh);break;
case Stage::ApplyFunctionReset:await(Stage::ReturnFunction);break;
case Stage::ReturnFunction:stage_=Stage::Complete;break;
case Stage::ReturnFunctionClear:stage_=Stage::Complete;break;
case Stage::ReturnRelease:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;void finish(Exit e){exit_=e;stage_=Stage::Complete;}Exit exit_=Exit::None;uint16_t event_=0;uint32_t record_=0,patch_=0;uint8_t key_=0,page_=0,other_=0,flags_=0,chord_=0,transpose_=0;bool childNonzero_=false;
};
}
