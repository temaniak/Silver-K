#pragma once
#include <cstdint>
namespace ks {
// Foreground panel work: drain control events, route encoder edits and gate
// periodic scans with the original wrapping 25-tick interval.
class StateForegroundTask final {
public:
 enum class Kind { Refresh, Controls, Encoder, Reset, Tick };enum class Stage { ClearRefresh, ReadRefreshFlags, SelectRefreshWord, SendRefreshWord, ReadDirty, SelectDirty, ClearDirty, WriteDirty, RefreshDirty, RefreshParts, RefreshClock, ReturnRefresh, PopControl, ClearControlIndex, CaptureControlIndex, LocateControl, TestControlKind, SelectControlEmpty, SelectControlRelease, ReadControlBias, ApplyControlBias, WriteControlValue, SelectControlPress, DispatchControlPress, RepeatControl, ReturnControl, NormalizeControlRelease, ReadControlValue, SelectControlReleaseValue, DispatchControlRelease, RepeatControlRelease, CheckEncoderContext, SelectEncoderContext, ReadEncoder, TestEncoder, SelectEncoder, ReadPage, CompareNamePage, SelectNamePage, CompareStorePage, SelectStorePage, ReadSpecialEdit, SelectSpecialEdit, ReadControlEdit, SelectControlEdit, BindControlDelta, BindControlKind, DispatchControlEdit, CheckParameterEdit, SelectParameterEdit, BindParameterDelta, BindParameterKind, DispatchParameterEdit, ReturnEncoder, ResetButtons, ResetControls, ResetNotes, ReturnReset, ReadTick, RetainTick, ReadLastTick, ElapsedTick, CompareTick, SelectTick, WriteLastTick, ScanButtons, StartAdc, ScanEncoder, ScanPedal, TestPedal, SelectPedal, SendPedal, RefreshPanel, ReturnTick, AwaitChild, Complete };
 enum class Read { RefreshFlags, Dirty, ControlBias, ControlValue, Page, SpecialEdit, ControlEdit, LastTick };enum class Write { Dirty, ControlValue, LastTick };
 explicit StateForegroundTask(Kind kind){switch(kind){case Kind::Refresh:stage_=Stage::ClearRefresh;break;case Kind::Controls:stage_=Stage::PopControl;break;case Kind::Encoder:stage_=Stage::CheckEncoderContext;break;case Kind::Reset:stage_=Stage::ResetButtons;break;case Kind::Tick:stage_=Stage::ReadTick;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint16_t event,bool accepted){if(!awaitingChild())return false;event_=event;accepted_=accepted;stage_=resume_;return true;}
 uint8_t refresh()const{return refresh_;}uint8_t dirty()const{return dirty_;}uint8_t index()const{return index_;}uint8_t bias()const{return bias_;}uint16_t event()const{return event_;}uint8_t page()const{return page_;}uint8_t special()const{return special_;}uint8_t control()const{return control_;}uint16_t last()const{return last_;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::SelectRefreshWord:case Stage::ReadControlValue:case Stage::CompareTick:return 6; case Stage::SelectNamePage:case Stage::SelectStorePage:case Stage::SelectSpecialEdit:case Stage::DispatchControlEdit:case Stage::SelectParameterEdit:case Stage::DispatchParameterEdit:return 8; case Stage::LocateControl:return 9; case Stage::ReadRefreshFlags:case Stage::ReadDirty:case Stage::ReadControlBias:case Stage::ReadPage:case Stage::ReadSpecialEdit:case Stage::ReadControlEdit:case Stage::ReadLastTick:case Stage::WriteLastTick:return 12; case Stage::ReturnRefresh:case Stage::ReturnControl:case Stage::ReturnEncoder:case Stage::ReturnReset:case Stage::ReturnTick:return 17; case Stage::SendRefreshWord:case Stage::RefreshDirty:case Stage::RefreshParts:case Stage::RefreshClock:case Stage::PopControl:case Stage::DispatchControlPress:case Stage::DispatchControlRelease:case Stage::CheckEncoderContext:case Stage::ReadEncoder:case Stage::CheckParameterEdit:case Stage::ResetButtons:case Stage::ResetControls:case Stage::ResetNotes:case Stage::ReadTick:case Stage::ScanButtons:case Stage::StartAdc:case Stage::ScanEncoder:case Stage::ScanPedal:case Stage::SendPedal:case Stage::RefreshPanel:return 20; case Stage::WriteDirty:return 9+store;case Stage::WriteControlValue:return 3+store;case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::SendRefreshWord:await(Stage::ReadDirty);break;
case Stage::RefreshDirty:await(Stage::RefreshParts);break;
case Stage::RefreshParts:await(Stage::RefreshClock);break;
case Stage::RefreshClock:await(Stage::ReturnRefresh);break;
case Stage::ReturnRefresh:stage_=Stage::Complete;break;
case Stage::PopControl:await(Stage::ClearControlIndex);break;
case Stage::DispatchControlPress:await(Stage::RepeatControl);break;
case Stage::RepeatControl:stage_=Stage::PopControl;break;
case Stage::ReturnControl:stage_=Stage::Complete;break;
case Stage::DispatchControlRelease:await(Stage::RepeatControlRelease);break;
case Stage::RepeatControlRelease:stage_=Stage::RepeatControl;break;
case Stage::CheckEncoderContext:await(Stage::SelectEncoderContext);break;
case Stage::ReadEncoder:await(Stage::TestEncoder);break;
case Stage::DispatchControlEdit:stage_=Stage::Complete;break;
case Stage::CheckParameterEdit:await(Stage::SelectParameterEdit);break;
case Stage::DispatchParameterEdit:stage_=Stage::Complete;break;
case Stage::ReturnEncoder:stage_=Stage::Complete;break;
case Stage::ResetButtons:await(Stage::ResetControls);break;
case Stage::ResetControls:await(Stage::ResetNotes);break;
case Stage::ResetNotes:await(Stage::ReturnReset);break;
case Stage::ReturnReset:stage_=Stage::Complete;break;
case Stage::ReadTick:await(Stage::RetainTick);break;
case Stage::ScanButtons:await(Stage::StartAdc);break;
case Stage::StartAdc:await(Stage::ScanEncoder);break;
case Stage::ScanEncoder:await(Stage::ScanPedal);break;
case Stage::ScanPedal:await(Stage::TestPedal);break;
case Stage::SendPedal:await(Stage::RefreshPanel);break;
case Stage::RefreshPanel:await(Stage::ReturnTick);break;
case Stage::ReturnTick:stage_=Stage::Complete;break;
case Stage::ReadRefreshFlags:refresh_=uint8_t(read(Read::RefreshFlags));next();break;
case Stage::ReadDirty:dirty_=uint8_t(read(Read::Dirty));next();break;
case Stage::SelectDirty:stage_=dirty_?Stage::ClearDirty:Stage::RefreshParts;break;
case Stage::WriteDirty:write(Write::Dirty,0);next();break;
case Stage::CaptureControlIndex:index_=uint8_t(event_);next();break;
case Stage::SelectControlEmpty:stage_=uint8_t(event_>>8)?Stage::SelectControlRelease:Stage::ReturnControl;break;
case Stage::SelectControlRelease:stage_=event_&0x8000?Stage::NormalizeControlRelease:Stage::ReadControlBias;break;
case Stage::ReadControlBias:bias_=uint8_t(read(Read::ControlBias));next();break;
case Stage::ApplyControlBias:event_=uint16_t((event_&0xff00)|uint8_t(uint8_t(event_)+bias_));next();break;
case Stage::WriteControlValue:write(Write::ControlValue,uint8_t(event_));next();break;
case Stage::SelectControlPress:stage_=event_&128?Stage::RepeatControl:Stage::DispatchControlPress;break;
case Stage::NormalizeControlRelease:event_&=0x7fff;next();break;
case Stage::ReadControlValue:event_=uint16_t((event_&0xff00)|uint8_t(read(Read::ControlValue)));next();break;
case Stage::SelectControlReleaseValue:stage_=event_&128?Stage::RepeatControl:Stage::DispatchControlRelease;break;
case Stage::SelectEncoderContext:stage_=accepted_?Stage::ReadEncoder:Stage::ReturnEncoder;break;
case Stage::SelectEncoder:stage_=uint8_t(event_)?Stage::ReadPage:Stage::ReturnEncoder;break;
case Stage::ReadPage:page_=uint8_t(read(Read::Page));next();break;
case Stage::SelectNamePage:stage_=page_==2?Stage::Complete:Stage::CompareStorePage;break;
case Stage::SelectStorePage:stage_=page_==3?Stage::Complete:Stage::ReadSpecialEdit;break;
case Stage::ReadSpecialEdit:special_=uint8_t(read(Read::SpecialEdit));next();break;
case Stage::SelectSpecialEdit:stage_=special_?Stage::Complete:Stage::ReadControlEdit;break;
case Stage::ReadControlEdit:control_=uint8_t(read(Read::ControlEdit));next();break;
case Stage::SelectControlEdit:stage_=control_?Stage::BindControlDelta:Stage::CheckParameterEdit;break;
case Stage::BindControlDelta:event_=uint16_t((event_&255)|(uint16_t(uint8_t(event_))<<8));next();break;
case Stage::BindControlKind:event_=uint16_t((event_&0xff00)|0xf0);next();break;
case Stage::SelectParameterEdit:stage_=accepted_?Stage::BindParameterDelta:Stage::Complete;break;
case Stage::BindParameterDelta:event_=uint16_t((event_&255)|(uint16_t(uint8_t(event_))<<8));next();break;
case Stage::BindParameterKind:event_=uint16_t((event_&0xff00)|0xf0);next();break;
case Stage::RetainTick:tick_=event_;next();break;
case Stage::ReadLastTick:last_=uint16_t(read(Read::LastTick));next();break;
case Stage::ElapsedTick:elapsed_=uint16_t(event_-last_);next();break;
case Stage::SelectTick:stage_=elapsed_<25?Stage::ReturnTick:Stage::WriteLastTick;break;
case Stage::WriteLastTick:write(Write::LastTick,tick_);next();break;
case Stage::SelectPedal:stage_=event_&0x8000?Stage::RefreshPanel:Stage::SendPedal;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint8_t refresh_=0,dirty_=0,index_=0,bias_=0,page_=0,special_=0,control_=0;uint16_t event_=0,tick_=0,last_=0,elapsed_=0;bool accepted_=false;
};
}
