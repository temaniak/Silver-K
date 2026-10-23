#pragma once
#include <cstdint>
namespace ks {
// System startup services, key-map patch, display glyphs/bar and live Flash signature lifecycle.
class SystemServicesTask final {
public:
 enum class Kind { Initialize, KeyMap, Keyboard, Glyphs, Bar, Signature, Save };enum class Stage { ResetHardware, PatchKeyMap, InitializeDisplay, InitializeButtons, InitializeUart, InitializeMidi, InitializeKeyboard, InitializeCalibration, InitializeDspLink, InitializeDspWait, InitializePanelLevel, InitializeControlMaps, EnableInterrupts, InitializeAdc, InitializeMidiJobs, InitializePlayback, InitializeClock, DisplayStartup, InitializeGlyphs, EnterPlayback, ReadHardware, SelectHardware, BindKeyMap, BindKeyMask, WriteKeyOne, WriteKeyTwo, SkipKeyThree, WriteKeyFour, ReturnKeyMap, BindKeyboardPort, WriteKeyboardPort, ResetKeyQueue, ReturnKeyboard, BindGlyphCommand, SetGlyphCommand, BindGlyphMask, BindGlyphCount, BindGlyphRow, BindRowCount, WriteGlyphRow, DecrementRowCount, RepeatGlyphRow, BindEmptyRow, WriteEmptyRow, BindFullRow, WriteFullRow, ShiftGlyphMask, DecrementGlyphCount, RepeatGlyph, BindLastRow, BindLastRowCount, WriteLastRow, DecrementLastRowCount, RepeatLastRow, ReturnGlyphs, RetainBarValue, CompareBarLimit, SelectBarLimit, ClampBarValue, BindBarPosition, SetBarPosition, BindFullBar, BindBarCount, BindBarUnit, SubtractBarUnit, SelectPartialBar, WriteSolidBar, DecrementSolidCount, RepeatSolidBar, JoinBarEnd, RestorePartialBar, BindPartialBar, WritePartialBar, DecrementPartialCount, SelectBlankBars, JoinPartialEnd, BindBlankBar, WriteBlankBar, DecrementBlankCount, RepeatBlankBar, BindBarEnd, CloseBar, BindSignatureSource, BindSignatureTarget, BindSignatureCount, ReadSignatureExpected, ReadSignatureActual, CompareSignature, SelectSignatureMismatch, DecrementSignatureCount, RepeatSignature, SelectSignatureMatch, ClearSignatureResult, JoinSignatureReturn, BindSignatureVersion, ReadSignatureVersion, ReturnSignature, BindSaveSource, BindSaveTarget, BindSaveCount, ReadSaveExpected, ReadSaveActual, CompareSave, SelectSaveMismatch, DecrementSaveCount, RepeatSave, SelectSaveMatch, BindWriteSource, BindWriteTarget, WriteSystemPage, ReturnSave, AwaitChild, Complete };
 enum class Read { Hardware, Expected, Actual, Version };enum class Write { KeyOne, KeyTwo, KeyFour, KeyboardPort };
 uint8_t hardware()const{return hardware_;}uint8_t expected()const{return expected_;}uint8_t actual()const{return actual_;}uint16_t version()const{return version_;}unsigned position()const{return position_;}uint8_t value()const{return value_;}uint16_t mask()const{return mask_;}
 explicit SystemServicesTask(Kind kind,uint8_t value=0){value_=value;switch(kind){case Kind::Initialize:stage_=Stage::ResetHardware;break;case Kind::KeyMap:stage_=Stage::ReadHardware;break;case Kind::Keyboard:stage_=Stage::BindKeyboardPort;break;case Kind::Glyphs:stage_=Stage::BindGlyphCommand;break;case Kind::Bar:stage_=Stage::RetainBarValue;break;case Kind::Signature:stage_=Stage::BindSignatureSource;break;case Kind::Save:stage_=Stage::BindSaveSource;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ResetHardware:case Stage::InitializeDisplay:case Stage::InitializeButtons:case Stage::InitializeUart:case Stage::InitializeMidi:case Stage::InitializeKeyboard:case Stage::InitializeCalibration:case Stage::InitializeDspLink:case Stage::InitializeDspWait:case Stage::InitializePanelLevel:case Stage::InitializeControlMaps:case Stage::InitializeAdc:case Stage::InitializeMidiJobs:case Stage::InitializePlayback:case Stage::InitializeClock:case Stage::DisplayStartup:case Stage::InitializeGlyphs:case Stage::ResetKeyQueue:case Stage::SetGlyphCommand:case Stage::WriteGlyphRow:case Stage::WriteEmptyRow:case Stage::WriteFullRow:case Stage::WriteLastRow:case Stage::SetBarPosition:case Stage::WriteSolidBar:case Stage::WritePartialBar:case Stage::WriteBlankBar:case Stage::WriteSystemPage:return 20; case Stage::PatchKeyMap:return 15; case Stage::EnableInterrupts:case Stage::BindGlyphMask:case Stage::BindGlyphCount:case Stage::BindSignatureCount:case Stage::BindSaveCount:return 6; case Stage::EnterPlayback:case Stage::CloseBar:case Stage::ReadSignatureExpected:case Stage::ReadSignatureActual:case Stage::ReadSaveExpected:case Stage::ReadSaveActual:return 8; case Stage::ReadHardware:return 12; case Stage::BindKeyMap:case Stage::BindSignatureSource:case Stage::BindSignatureTarget:case Stage::BindSignatureVersion:case Stage::BindSaveSource:case Stage::BindSaveTarget:case Stage::BindWriteSource:case Stage::BindWriteTarget:return 9; case Stage::WriteKeyOne:case Stage::WriteKeyTwo:case Stage::WriteKeyFour:return 5+store; case Stage::ReturnKeyMap:case Stage::ReturnKeyboard:case Stage::ReturnGlyphs:case Stage::ReturnSignature:case Stage::ReturnSave:return 17; case Stage::WriteKeyboardPort:return 3+store; case Stage::ReadSignatureVersion:return 18; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::EnterPlayback:stage_=Stage::Complete;break;
case Stage::ReadHardware:hardware_=uint8_t(read(Read::Hardware,0));next();break;
case Stage::SelectHardware:stage_=hardware_?Stage::BindKeyMap:Stage::ReturnKeyMap;break;
case Stage::WriteKeyOne:write(Write::KeyOne,255);next();break;
case Stage::WriteKeyTwo:write(Write::KeyTwo,255);next();break;
case Stage::WriteKeyFour:write(Write::KeyFour,255);next();break;
case Stage::WriteKeyboardPort:write(Write::KeyboardPort,1);next();break;
case Stage::BindGlyphMask:mask_=0x3e0;next();break;
case Stage::BindGlyphCount:columns_=6;next();break;
case Stage::BindRowCount:rows_=6;next();break;
case Stage::DecrementRowCount:--rows_;next();break;
case Stage::RepeatGlyphRow:stage_=rows_?Stage::WriteGlyphRow:Stage::BindEmptyRow;break;
case Stage::ShiftGlyphMask:mask_>>=1;next();break;
case Stage::DecrementGlyphCount:--columns_;next();break;
case Stage::RepeatGlyph:stage_=columns_?Stage::BindGlyphRow:Stage::BindLastRow;break;
case Stage::BindLastRowCount:rows_=8;next();break;
case Stage::DecrementLastRowCount:--rows_;next();break;
case Stage::RepeatLastRow:stage_=rows_?Stage::WriteLastRow:Stage::ReturnGlyphs;break;
case Stage::SelectBarLimit:stage_=value_<=25?Stage::BindBarPosition:Stage::ClampBarValue;break;
case Stage::ClampBarValue:value_=25;next();break;
case Stage::BindBarCount:rows_=5;next();break;
case Stage::SubtractBarUnit:borrow_=value_<5;value_=uint8_t(value_-5);next();break;
case Stage::SelectPartialBar:stage_=borrow_?Stage::RestorePartialBar:Stage::WriteSolidBar;break;
case Stage::DecrementSolidCount:--rows_;next();break;
case Stage::RepeatSolidBar:stage_=rows_?Stage::SubtractBarUnit:Stage::JoinBarEnd;break;
case Stage::JoinBarEnd:stage_=Stage::BindBarEnd;break;
case Stage::RestorePartialBar:value_=uint8_t(value_+5);next();break;
case Stage::DecrementPartialCount:--rows_;next();break;
case Stage::SelectBlankBars:stage_=rows_?Stage::BindBlankBar:Stage::JoinPartialEnd;break;
case Stage::JoinPartialEnd:stage_=Stage::BindBarEnd;break;
case Stage::DecrementBlankCount:--rows_;next();break;
case Stage::RepeatBlankBar:stage_=rows_?Stage::WriteBlankBar:Stage::BindBarEnd;break;
case Stage::CloseBar:stage_=Stage::Complete;break;
case Stage::BindSignatureCount:position_=0;limit_=40;matched_=true;next();break;
case Stage::ReadSignatureExpected:expected_=uint8_t(read(Read::Expected,position_));next();break;
case Stage::ReadSignatureActual:actual_=uint8_t(read(Read::Actual,position_));++position_;next();break;
case Stage::SelectSignatureMismatch:if(expected_!=actual_){matched_=false;stage_=Stage::SelectSignatureMatch;}else next();break;
case Stage::RepeatSignature:stage_=position_<limit_?Stage::ReadSignatureExpected:Stage::SelectSignatureMatch;break;
case Stage::SelectSignatureMatch:stage_=matched_?Stage::BindSignatureVersion:Stage::ClearSignatureResult;break;
case Stage::JoinSignatureReturn:stage_=Stage::ReturnSignature;break;
case Stage::ReadSignatureVersion:version_=uint16_t(read(Read::Version,0));next();break;
case Stage::BindSaveCount:position_=0;limit_=48;matched_=true;next();break;
case Stage::ReadSaveExpected:expected_=uint8_t(read(Read::Expected,position_));next();break;
case Stage::ReadSaveActual:actual_=uint8_t(read(Read::Actual,position_));++position_;next();break;
case Stage::SelectSaveMismatch:if(expected_!=actual_){matched_=false;stage_=Stage::SelectSaveMatch;}else next();break;
case Stage::RepeatSave:stage_=position_<limit_?Stage::ReadSaveExpected:Stage::SelectSaveMatch;break;
case Stage::SelectSaveMatch:stage_=matched_?Stage::ReturnSave:Stage::BindWriteSource;break;
case Stage::ResetHardware:await(Stage::PatchKeyMap);break;
case Stage::PatchKeyMap:await(Stage::InitializeDisplay);break;
case Stage::InitializeDisplay:await(Stage::InitializeButtons);break;
case Stage::InitializeButtons:await(Stage::InitializeUart);break;
case Stage::InitializeUart:await(Stage::InitializeMidi);break;
case Stage::InitializeMidi:await(Stage::InitializeKeyboard);break;
case Stage::InitializeKeyboard:await(Stage::InitializeCalibration);break;
case Stage::InitializeCalibration:await(Stage::InitializeDspLink);break;
case Stage::InitializeDspLink:await(Stage::InitializeDspWait);break;
case Stage::InitializeDspWait:await(Stage::InitializePanelLevel);break;
case Stage::InitializePanelLevel:await(Stage::InitializeControlMaps);break;
case Stage::InitializeControlMaps:await(Stage::EnableInterrupts);break;
case Stage::InitializeAdc:await(Stage::InitializeMidiJobs);break;
case Stage::InitializeMidiJobs:await(Stage::InitializePlayback);break;
case Stage::InitializePlayback:await(Stage::InitializeClock);break;
case Stage::InitializeClock:await(Stage::DisplayStartup);break;
case Stage::DisplayStartup:await(Stage::InitializeGlyphs);break;
case Stage::InitializeGlyphs:await(Stage::EnterPlayback);break;
case Stage::ReturnKeyMap:stage_=Stage::Complete;break;
case Stage::ResetKeyQueue:await(Stage::ReturnKeyboard);break;
case Stage::ReturnKeyboard:stage_=Stage::Complete;break;
case Stage::SetGlyphCommand:await(Stage::BindGlyphMask);break;
case Stage::WriteGlyphRow:await(Stage::DecrementRowCount);break;
case Stage::WriteEmptyRow:await(Stage::BindFullRow);break;
case Stage::WriteFullRow:await(Stage::ShiftGlyphMask);break;
case Stage::WriteLastRow:await(Stage::DecrementLastRowCount);break;
case Stage::ReturnGlyphs:stage_=Stage::Complete;break;
case Stage::SetBarPosition:await(Stage::BindFullBar);break;
case Stage::WriteSolidBar:await(Stage::DecrementSolidCount);break;
case Stage::WritePartialBar:await(Stage::DecrementPartialCount);break;
case Stage::WriteBlankBar:await(Stage::DecrementBlankCount);break;
case Stage::ReturnSignature:stage_=Stage::Complete;break;
case Stage::WriteSystemPage:await(Stage::ReturnSave);break;
case Stage::ReturnSave:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint8_t value_=0,hardware_=0,expected_=0,actual_=0,rows_=0,columns_=0;uint16_t version_=0,mask_=0;unsigned position_=0,limit_=0;bool matched_=true,borrow_=false;
};
}
