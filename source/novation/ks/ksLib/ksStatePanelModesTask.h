#pragma once
#include <cstdint>
namespace ks {
// Panel mode/octave/transpose contacts, signed clamps, five-part reset and semitone publication.
class StatePanelModesTask final {
public:
 enum class Kind { ModeKey, OctaveDown, RepeatDown, OctaveUp, RepeatUp, Reference, TransposeDown, TransposeUp, ApplyTranspose, ResetVoices, Semitone, CheckSemitone };enum class Stage { SelectModeKey, ReleaseModeKey, JoinModeKey, PressModeKey, RefreshModeKey, SelectOctaveDown, BindOctaveDownDelay, JoinOctaveDown, BindOctaveDownRepeat, BindOctaveDownCallback, ScheduleOctaveDown, ClearOctaveDownIndex, BindOctaveDownIndex, ReadOctaveDownBase, LocateOctaveDown, ReadOctaveDown, DecrementOctaveDown, SelectOctaveDownLimit, ClampOctaveDown, ApplyOctaveDown, ReadOctaveDownScreen, ObserveOctaveDownEdit, SelectOctaveDownEdit, ReturnOctaveDown, SelectOctaveUp, BindOctaveUpDelay, JoinOctaveUp, BindOctaveUpRepeat, BindOctaveUpCallback, ScheduleOctaveUp, ClearOctaveUpIndex, BindOctaveUpIndex, ReadOctaveUpBase, LocateOctaveUp, ReadOctaveUp, IncrementOctaveUp, TestOctaveUpLimit, SelectOctaveUpLimit, ClampOctaveUp, ApplyOctaveUp, ReadOctaveUpScreen, ObserveOctaveUpEdit, SelectOctaveUpEdit, ReturnOctaveUp, ClearTransposeReference, BindTransposeIndex, LocateTransposeReference, ReturnTransposeReference, ReadTransposeDownHardware, SelectTransposeDownHardware, TestTransposeDownEvent, SelectTransposeDownEvent, ReadTransposeToggle, FlipTransposeToggle, WriteTransposeToggle, RefreshTransposeToggle, PublishTransposeToggle, TestTransposeDownContact, SelectTransposeDownContact, ClearTransposeDownContact, ReturnTransposeDownRelease, SetTransposeDownContact, LookupTransposeDown, BindTransposeDownZero, ReadTransposeDownOther, SelectTransposeDownOther, ReadTransposeDown, DecrementTransposeDown, TestTransposeDownLimit, SelectTransposeDownLimit, ClampTransposeDown, JoinTransposeDown, ReadTransposeUpHardware, SelectTransposeUpHardware, TestTransposeUpEvent, SelectTransposeUpEvent, EnterTransposeUp, TestTransposeUpContact, SelectTransposeUpContact, ClearTransposeUpContact, ReturnTransposeUpRelease, SetTransposeUpContact, LookupTransposeUp, BindTransposeUpZero, ReadTransposeUpOther, SelectTransposeUpOther, BindTransposeUpLimit, ReadTransposeHardwareType, TestTransposeHardwareType, SelectTransposeHardwareType, BindTransposeAlternateLimit, ReadTransposeUp, IncrementTransposeUp, TestTransposeUpLimit, SelectTransposeUpLimit, ClampTransposeUp, BindTransposeContext, WriteTransposeContext, ApplyTranspose, ClearTransposeContext, ResetTransposeContext, ConvertTranspose, BindTransposeNotice, ShowTransposeNotice, BindTransposeBase, ReadTransposeValue, ShowTransposeValue, ReturnTranspose, ReadResetMode, ReadResetPart, SaveResetContext, BindResetMode, WriteResetMode, BindResetPart, BindResetCount, WriteResetPart, SaveResetPart, ResetPartMidi, ResetPartVoices, ResetPartQueue, BindResetMulti, WriteResetMulti, RestoreResetPart, NextResetPart, DecrementResetCount, RepeatResetPart, RestoreResetContext, WriteRestoredPart, WriteRestoredMode, ReturnReset, ReadTransposeType, TestTransposeType, SelectTransposeType, IncrementTransposeValue, BindSemitoneScale, ScaleTranspose, ReadOldSemitone, WriteOldSemitone, WriteNewSemitone, ReturnSemitone, ReadCurrentSemitone, ReadPreviousSemitone, CompareSemitones, SelectSemitoneReset, ReturnSemitoneCheck, AwaitChild, Complete };
 enum class Read { Patch, Screen, Hardware, Toggle, Transpose, Type, Mode, Part, Semitone, PreviousSemitone, Byte, Flags };enum class Write { Toggle, Context, Mode, Part, Semitone, PreviousSemitone, Flags };enum class Exit { None, ReleaseNavigation, Refresh, ApplyScreen, Release, PublishToggle, NoteOff, NoteOn, ResetVoices };Exit exit()const{return exit_;}uint32_t hardware()const{return hardware_;}uint32_t screen()const{return screen_;}uint32_t value()const{return value_;}uint32_t semitone()const{return semitone_;}uint32_t patch()const{return patch_;}uint32_t type()const{return type_;}uint32_t mode()const{return mode_;}uint32_t oldSemitone()const{return oldSemitone_;}uint32_t flags()const{return flags_;}uint32_t savedPart()const{return savedPart_;}uint32_t record()const{return record_;}uint32_t toggle()const{return toggle_;}
 explicit StatePanelModesTask(Kind kind,uint16_t event=0,bool pressed=true,uint32_t record=0){event_=event;value_=uint8_t(event>>8);pressed_=pressed;record_=record;switch(kind){case Kind::ModeKey:stage_=Stage::SelectModeKey;break;case Kind::OctaveDown:stage_=Stage::SelectOctaveDown;break;case Kind::RepeatDown:stage_=Stage::BindOctaveDownRepeat;break;case Kind::OctaveUp:stage_=Stage::SelectOctaveUp;break;case Kind::RepeatUp:stage_=Stage::BindOctaveUpRepeat;break;case Kind::Reference:stage_=Stage::ClearTransposeReference;break;case Kind::TransposeDown:stage_=Stage::ReadTransposeDownHardware;break;case Kind::TransposeUp:stage_=Stage::ReadTransposeUpHardware;break;case Kind::ApplyTranspose:stage_=Stage::BindTransposeContext;break;case Kind::ResetVoices:stage_=Stage::ReadResetMode;break;case Kind::Semitone:stage_=Stage::ReadTransposeType;break;case Kind::CheckSemitone:stage_=Stage::ReadCurrentSemitone;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(bool nonzero){if(!awaitingChild())return false;childNonzero_=nonzero;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ReleaseModeKey:case Stage::PressModeKey:case Stage::ScheduleOctaveDown:case Stage::ApplyOctaveDown:case Stage::ObserveOctaveDownEdit:case Stage::ScheduleOctaveUp:case Stage::ApplyOctaveUp:case Stage::ObserveOctaveUpEdit:case Stage::RefreshTransposeToggle:case Stage::LookupTransposeUp:case Stage::ApplyTranspose:case Stage::ShowTransposeNotice:case Stage::ShowTransposeValue:case Stage::ResetPartMidi:case Stage::ResetPartVoices:case Stage::ResetPartQueue:return 20; case Stage::RefreshModeKey:case Stage::SelectOctaveDown:case Stage::SelectOctaveDownEdit:case Stage::SelectOctaveUp:case Stage::SelectOctaveUpEdit:case Stage::SelectTransposeDownEvent:case Stage::PublishTransposeToggle:case Stage::SelectTransposeUpEvent:case Stage::EnterTransposeUp:case Stage::SelectSemitoneReset:return 8; case Stage::BindOctaveDownDelay:case Stage::BindOctaveDownRepeat:case Stage::ReadOctaveDown:case Stage::BindOctaveUpDelay:case Stage::BindOctaveUpRepeat:case Stage::ReadOctaveUp:case Stage::ReadTransposeDown:case Stage::ReadTransposeUp:return 6; case Stage::BindOctaveDownCallback:case Stage::BindOctaveUpCallback:case Stage::LocateTransposeReference:case Stage::ReadTransposeDownOther:case Stage::ReadTransposeUpOther:case Stage::BindTransposeContext:case Stage::BindTransposeNotice:case Stage::BindTransposeBase:return 9; case Stage::ReadOctaveDownBase:case Stage::ReadOctaveUpBase:case Stage::WriteTransposeContext:case Stage::ResetTransposeContext:case Stage::ScaleTranspose:return 18; case Stage::ReadOctaveDownScreen:case Stage::ReadOctaveUpScreen:case Stage::ReadTransposeDownHardware:case Stage::ReadTransposeToggle:case Stage::ReadTransposeUpHardware:case Stage::ReadTransposeHardwareType:case Stage::ReadResetMode:case Stage::ReadResetPart:case Stage::ReadTransposeType:case Stage::ReadOldSemitone:case Stage::ReadCurrentSemitone:case Stage::ReadPreviousSemitone:return 12; case Stage::ReturnOctaveDown:case Stage::ReturnOctaveUp:case Stage::ReturnTransposeReference:case Stage::ReturnTransposeDownRelease:case Stage::ReturnTransposeUpRelease:case Stage::ReturnTranspose:case Stage::ReturnReset:case Stage::ReturnSemitone:case Stage::ReturnSemitoneCheck:return 17; case Stage::WriteTransposeToggle:case Stage::ClearTransposeDownContact:case Stage::SetTransposeDownContact:case Stage::ClearTransposeUpContact:case Stage::SetTransposeUpContact:case Stage::WriteResetMode:case Stage::WriteResetPart:case Stage::WriteResetMulti:case Stage::WriteRestoredPart:case Stage::WriteRestoredMode:case Stage::WriteOldSemitone:case Stage::WriteNewSemitone:return 9+store; case Stage::LookupTransposeDown:case Stage::ConvertTranspose:case Stage::ReadTransposeValue:return 15; case Stage::SaveResetContext:case Stage::SaveResetPart:case Stage::RestoreResetPart:case Stage::RestoreResetContext:return 11; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::SelectModeKey:stage_=pressed_?Stage::PressModeKey:Stage::ReleaseModeKey;break;
case Stage::JoinModeKey:stage_=Stage::RefreshModeKey;break;
case Stage::RefreshModeKey:finish(Exit::Refresh);break;
case Stage::SelectOctaveDown:if(!pressed_)finish(Exit::ReleaseNavigation);else next();break;
case Stage::JoinOctaveDown:stage_=Stage::BindOctaveDownCallback;break;
case Stage::ReadOctaveDownBase:patch_=read(Read::Patch,0);next();break;
case Stage::ReadOctaveDown:value_=uint8_t(read(Read::Byte,patch_+226));next();break;
case Stage::DecrementOctaveDown:negative_=int(int8_t(value_))-1<0;value_=uint8_t(value_-1);next();break;
case Stage::SelectOctaveDownLimit:stage_=negative_?Stage::ClampOctaveDown:Stage::ApplyOctaveDown;break;
case Stage::ClampOctaveDown:value_=6;next();break;
case Stage::ReadOctaveDownScreen:screen_=uint8_t(read(Read::Screen,0));next();break;
case Stage::SelectOctaveDownEdit:if(childNonzero_)finish(Exit::ApplyScreen);else next();break;
case Stage::SelectOctaveUp:if(!pressed_)finish(Exit::ReleaseNavigation);else next();break;
case Stage::JoinOctaveUp:stage_=Stage::BindOctaveUpCallback;break;
case Stage::ReadOctaveUpBase:patch_=read(Read::Patch,0);next();break;
case Stage::ReadOctaveUp:value_=uint8_t(read(Read::Byte,patch_+226));next();break;
case Stage::IncrementOctaveUp:++value_;next();break;
case Stage::SelectOctaveUpLimit:stage_=value_<=6?Stage::ApplyOctaveUp:Stage::ClampOctaveUp;break;
case Stage::ClampOctaveUp:value_=0;next();break;
case Stage::ReadOctaveUpScreen:screen_=uint8_t(read(Read::Screen,0));next();break;
case Stage::SelectOctaveUpEdit:if(childNonzero_)finish(Exit::ApplyScreen);else next();break;
case Stage::ReadTransposeDownHardware:hardware_=uint8_t(read(Read::Hardware,0));next();break;
case Stage::SelectTransposeDownHardware:stage_=hardware_?Stage::TestTransposeDownEvent:Stage::TestTransposeDownContact;break;
case Stage::SelectTransposeDownEvent:if(!(event_>>8))finish(Exit::Release);else next();break;
case Stage::ReadTransposeToggle:toggle_=uint8_t(read(Read::Toggle,0));next();break;
case Stage::FlipTransposeToggle:toggle_^=1;next();break;
case Stage::WriteTransposeToggle:write(Write::Toggle,0,toggle_);next();break;
case Stage::PublishTransposeToggle:finish(Exit::PublishToggle);break;
case Stage::SelectTransposeDownContact:stage_=event_>>8?Stage::SetTransposeDownContact:Stage::ClearTransposeDownContact;break;
case Stage::ClearTransposeDownContact:flags_=uint8_t(read(Read::Flags,record_))&uint8_t(~64);write(Write::Flags,record_,flags_);next();break;
case Stage::SetTransposeDownContact:flags_=uint8_t(read(Read::Flags,record_))|64;write(Write::Flags,record_,flags_);next();break;
case Stage::BindTransposeDownZero:value_=0;next();break;
case Stage::ReadTransposeDownOther:flags_=uint8_t(read(Read::Flags,record_));next();break;
case Stage::SelectTransposeDownOther:stage_=flags_&128?Stage::JoinTransposeDown:Stage::ReadTransposeDown;break;
case Stage::ReadTransposeDown:value_=uint8_t(read(Read::Transpose,0));next();break;
case Stage::DecrementTransposeDown:--value_;next();break;
case Stage::SelectTransposeDownLimit:stage_=int8_t(value_)>=-3?Stage::JoinTransposeDown:Stage::ClampTransposeDown;break;
case Stage::ClampTransposeDown:value_=253;next();break;
case Stage::JoinTransposeDown:stage_=Stage::BindTransposeContext;break;
case Stage::ReadTransposeUpHardware:hardware_=uint8_t(read(Read::Hardware,0));next();break;
case Stage::SelectTransposeUpHardware:stage_=hardware_?Stage::TestTransposeUpEvent:Stage::TestTransposeUpContact;break;
case Stage::SelectTransposeUpEvent:if(!(event_>>8))finish(Exit::NoteOff);else next();break;
case Stage::EnterTransposeUp:finish(Exit::NoteOn);break;
case Stage::SelectTransposeUpContact:stage_=event_>>8?Stage::SetTransposeUpContact:Stage::ClearTransposeUpContact;break;
case Stage::ClearTransposeUpContact:flags_=uint8_t(read(Read::Flags,record_))&uint8_t(~128);write(Write::Flags,record_,flags_);next();break;
case Stage::SetTransposeUpContact:flags_=uint8_t(read(Read::Flags,record_))|128;write(Write::Flags,record_,flags_);next();break;
case Stage::BindTransposeUpZero:value_=0;next();break;
case Stage::ReadTransposeUpOther:flags_=uint8_t(read(Read::Flags,record_));next();break;
case Stage::SelectTransposeUpOther:stage_=flags_&64?Stage::BindTransposeContext:Stage::BindTransposeUpLimit;break;
case Stage::BindTransposeUpLimit:limit_=4;next();break;
case Stage::ReadTransposeHardwareType:type_=uint8_t(read(Read::Type,0));next();break;
case Stage::SelectTransposeHardwareType:stage_=type_==4?Stage::ReadTransposeUp:Stage::BindTransposeAlternateLimit;break;
case Stage::BindTransposeAlternateLimit:limit_=3;next();break;
case Stage::ReadTransposeUp:value_=uint8_t(read(Read::Transpose,0));next();break;
case Stage::IncrementTransposeUp:++value_;next();break;
case Stage::SelectTransposeUpLimit:stage_=int8_t(value_)<=int8_t(limit_)?Stage::BindTransposeContext:Stage::ClampTransposeUp;break;
case Stage::ClampTransposeUp:value_=limit_;next();break;
case Stage::WriteTransposeContext:write(Write::Context,0,0x168fc);next();break;
case Stage::ResetTransposeContext:write(Write::Context,0,0);next();break;
case Stage::ReadTransposeValue:value_=uint8_t(read(Read::Transpose,0));next();break;
case Stage::ReadResetMode:mode_=uint8_t(read(Read::Mode,0));next();break;
case Stage::ReadResetPart:savedPart_=uint8_t(read(Read::Part,0));next();break;
case Stage::WriteResetMode:write(Write::Mode,0,0);next();break;
case Stage::BindResetPart:part_=0;next();break;
case Stage::BindResetCount:remaining_=5;next();break;
case Stage::WriteResetPart:write(Write::Part,0,part_);next();break;
case Stage::WriteResetMulti:write(Write::Mode,0,1);next();break;
case Stage::NextResetPart:++part_;next();break;
case Stage::DecrementResetCount:--remaining_;next();break;
case Stage::RepeatResetPart:stage_=remaining_?Stage::WriteResetPart:Stage::RestoreResetContext;break;
case Stage::WriteRestoredPart:write(Write::Part,0,savedPart_);next();break;
case Stage::WriteRestoredMode:write(Write::Mode,0,mode_);next();break;
case Stage::ReadTransposeType:type_=uint8_t(read(Read::Type,0));next();break;
case Stage::SelectTransposeType:stage_=type_==4?Stage::BindSemitoneScale:Stage::IncrementTransposeValue;break;
case Stage::IncrementTransposeValue:++value_;next();break;
case Stage::ScaleTranspose:semitone_=uint8_t(int(int8_t(value_))*12);next();break;
case Stage::ReadOldSemitone:oldSemitone_=uint8_t(read(Read::Semitone,0));next();break;
case Stage::WriteOldSemitone:write(Write::PreviousSemitone,0,oldSemitone_);next();break;
case Stage::WriteNewSemitone:write(Write::Semitone,0,semitone_);next();break;
case Stage::ReadCurrentSemitone:semitone_=uint8_t(read(Read::Semitone,0));next();break;
case Stage::ReadPreviousSemitone:oldSemitone_=uint8_t(read(Read::PreviousSemitone,0));next();break;
case Stage::SelectSemitoneReset:if(semitone_!=oldSemitone_)finish(Exit::ResetVoices);else next();break;
case Stage::ReleaseModeKey:await(Stage::JoinModeKey);break;
case Stage::PressModeKey:await(Stage::RefreshModeKey);break;
case Stage::ScheduleOctaveDown:await(Stage::ClearOctaveDownIndex);break;
case Stage::ApplyOctaveDown:await(Stage::ReadOctaveDownScreen);break;
case Stage::ObserveOctaveDownEdit:await(Stage::SelectOctaveDownEdit);break;
case Stage::ReturnOctaveDown:stage_=Stage::Complete;break;
case Stage::ScheduleOctaveUp:await(Stage::ClearOctaveUpIndex);break;
case Stage::ApplyOctaveUp:await(Stage::ReadOctaveUpScreen);break;
case Stage::ObserveOctaveUpEdit:await(Stage::SelectOctaveUpEdit);break;
case Stage::ReturnOctaveUp:stage_=Stage::Complete;break;
case Stage::ReturnTransposeReference:stage_=Stage::Complete;break;
case Stage::RefreshTransposeToggle:await(Stage::PublishTransposeToggle);break;
case Stage::ReturnTransposeDownRelease:stage_=Stage::Complete;break;
case Stage::LookupTransposeDown:await(Stage::BindTransposeDownZero);break;
case Stage::ReturnTransposeUpRelease:stage_=Stage::Complete;break;
case Stage::LookupTransposeUp:await(Stage::BindTransposeUpZero);break;
case Stage::ApplyTranspose:await(Stage::ClearTransposeContext);break;
case Stage::ConvertTranspose:await(Stage::BindTransposeNotice);break;
case Stage::ShowTransposeNotice:await(Stage::BindTransposeBase);break;
case Stage::ShowTransposeValue:await(Stage::ReturnTranspose);break;
case Stage::ReturnTranspose:stage_=Stage::Complete;break;
case Stage::ResetPartMidi:await(Stage::ResetPartVoices);break;
case Stage::ResetPartVoices:await(Stage::ResetPartQueue);break;
case Stage::ResetPartQueue:await(Stage::BindResetMulti);break;
case Stage::ReturnReset:stage_=Stage::Complete;break;
case Stage::ReturnSemitone:stage_=Stage::Complete;break;
case Stage::ReturnSemitoneCheck:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;void finish(Exit e){exit_=e;stage_=Stage::Complete;}Exit exit_=Exit::None;uint32_t patch_=0,record_=0;uint16_t event_=0;uint8_t value_=0,screen_=0,hardware_=0,toggle_=0,flags_=0,limit_=0,type_=0,mode_=0,savedPart_=0,part_=0,remaining_=0,semitone_=0,oldSemitone_=0;bool pressed_=true,negative_=false,childNonzero_=false;
};
}
