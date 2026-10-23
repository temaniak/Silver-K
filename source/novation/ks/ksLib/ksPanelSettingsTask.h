#pragma once
#include <cstdint>
namespace ks {
// Panel calibration/settings lifecycle, live notices, range helpers, copied calibration and saturated encoder duty.
class PanelSettingsTask final {
public:
 enum class Kind { Reset, Select, Show, Poll, Coefficients, First, Second, Load, Save };enum class Stage { LoadCalibration, ClearNotice, WriteClearedNotice, ReturnReset, WriteSelectedMode, ShowSelectedMode, ReadNotice, SelectNotice, SaveNoticeInput, BindNoticeText, PrintNoticeText, RestoreNoticeInput, SetNotice, WriteNotice, ReadSelectedMode, CompareModeFour, SelectModeFour, CompareModeFive, SelectModeFive, ReturnInactive, SaveModeInput, BindModePosition, SetModePosition, PrintModeValue, RestoreModeInput, ReturnMode, PollInput, TestPending, SelectPending, ShowInput, CompareFirstSelector, SelectFirstSelector, UpdateFirstRange, SaveFirstInput, BindFirstPosition, SetFirstPosition, RestoreFirstInput, ScaleFirst, BindFirstValue, DisplayFirstValue, JoinFirst, CompareSecondSelector, SelectSecondSelector, UpdateSecondRange, SaveSecondInput, BindSecondPosition, SetSecondPosition, RestoreSecondInput, ScaleSecond, BindSecondValue, DisplaySecondValue, JoinSecond, CompareThirdSelector, SelectThirdSelector, SaveThirdInput, BindThirdPosition, SetThirdPosition, RestoreThirdInput, ScaleThird, BindThirdValue, DisplayThirdValue, ReadEncoder, TestEncoder, SelectEncoder, ReadDuty, NormalizeDuty, RetainDelta, ExtendDelta, AddDuty, SelectDutySign, ClearDuty, JoinDuty, CompareDutyLimit, SelectDutyLimit, ClampDuty, WriteDuty, ApplyDuty, BindDutyPosition, SetDutyPosition, DisplayDuty, ReturnPoll, BindFirstHistory, BindFirstCoefficients, BindFirstBias, CalculateFirstCoefficients, BindSecondHistory, BindSecondCoefficients, BindSecondBias, CalculateSecondCoefficients, SaveCoefficients, BindFirstRange, UpdateFirstHistory, BindFirstOutput, BindFirstOutputBias, CalculateFirstOutput, ReturnFirst, BindSecondRange, InvertSecondInput, UpdateSecondHistory, RestoreInvertedInput, BindSecondOutput, BindSecondOutputBias, CalculateSecondOutput, ReturnSecond, BindLoadSource, BindLoadTarget, BindCopyCount, ReadCopyByte, WriteCopyByte, NextCopyTarget, DecrementCopyCount, RepeatCopy, BindHistoryInitial, WriteFirstHistory, WriteSecondHistory, ReadLoadedDuty, ApplyLoadedDuty, ReturnLoad, BindSaveTarget, BindSaveSource, SaveCalibration, ReturnSave, AwaitChild, Complete };
 enum class Read { Notice, Mode, Duty, CalibrationByte };enum class Write { Notice, Mode, Duty, CalibrationByte, FirstHistory, SecondHistory };
 struct Reply {uint8_t selector,delta;bool pending;};uint8_t notice()const{return notice_;}uint8_t mode()const{return mode_;}uint8_t duty()const{return duty_;}uint8_t byte()const{return byte_;}unsigned position()const{return position_;}
 explicit PanelSettingsTask(Kind kind,uint8_t selector=0){selector_=selector;switch(kind){case Kind::Reset:stage_=Stage::LoadCalibration;break;case Kind::Select:stage_=Stage::WriteSelectedMode;break;case Kind::Show:stage_=Stage::ReadNotice;break;case Kind::Poll:stage_=Stage::PollInput;break;case Kind::Coefficients:stage_=Stage::BindFirstHistory;break;case Kind::First:stage_=Stage::BindFirstRange;break;case Kind::Second:stage_=Stage::BindSecondRange;break;case Kind::Load:stage_=Stage::BindLoadSource;break;case Kind::Save:stage_=Stage::BindSaveTarget;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(Reply reply){if(!awaitingChild())return false;if(resume_==Stage::TestPending){selector_=reply.selector;pending_=reply.pending;}if(resume_==Stage::TestEncoder)delta_=reply.selector;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::LoadCalibration:case Stage::PrintNoticeText:case Stage::SetModePosition:case Stage::PrintModeValue:case Stage::PollInput:case Stage::UpdateFirstRange:case Stage::SetFirstPosition:case Stage::ScaleFirst:case Stage::DisplayFirstValue:case Stage::UpdateSecondRange:case Stage::SetSecondPosition:case Stage::ScaleSecond:case Stage::DisplaySecondValue:case Stage::SetThirdPosition:case Stage::ScaleThird:case Stage::DisplayThirdValue:case Stage::ReadEncoder:case Stage::ApplyDuty:case Stage::SetDutyPosition:case Stage::DisplayDuty:case Stage::ApplyLoadedDuty:case Stage::SaveCalibration:return 20; case Stage::WriteClearedNotice:case Stage::WriteSelectedMode:case Stage::WriteNotice:case Stage::WriteDuty:return 9+store; case Stage::ReturnReset:case Stage::ReturnInactive:case Stage::ReturnMode:case Stage::ReturnPoll:case Stage::ReturnFirst:case Stage::ReturnSecond:case Stage::ReturnLoad:case Stage::ReturnSave:return 17; case Stage::ShowSelectedMode:case Stage::ShowInput:case Stage::CalculateFirstCoefficients:case Stage::CalculateSecondCoefficients:case Stage::UpdateFirstHistory:case Stage::CalculateFirstOutput:case Stage::UpdateSecondHistory:case Stage::CalculateSecondOutput:return 15; case Stage::ReadNotice:case Stage::ReadSelectedMode:case Stage::ReadDuty:case Stage::WriteFirstHistory:case Stage::WriteSecondHistory:case Stage::ReadLoadedDuty:return 12; case Stage::SaveNoticeInput:case Stage::RestoreNoticeInput:case Stage::SaveModeInput:case Stage::RestoreModeInput:case Stage::SaveFirstInput:case Stage::RestoreFirstInput:case Stage::SaveSecondInput:case Stage::RestoreSecondInput:case Stage::SaveThirdInput:case Stage::RestoreThirdInput:return 11; case Stage::BindNoticeText:case Stage::BindFirstHistory:case Stage::BindFirstCoefficients:case Stage::BindSecondHistory:case Stage::BindSecondCoefficients:case Stage::BindFirstRange:case Stage::BindFirstOutput:case Stage::BindSecondRange:case Stage::BindSecondOutput:case Stage::BindLoadSource:case Stage::BindLoadTarget:case Stage::BindSaveTarget:case Stage::BindSaveSource:return 9; case Stage::CompareDutyLimit:case Stage::BindFirstBias:case Stage::BindSecondBias:case Stage::BindFirstOutputBias:case Stage::BindSecondOutputBias:case Stage::BindCopyCount:case Stage::BindHistoryInitial:return 6; case Stage::SaveCoefficients:case Stage::ReadCopyByte:return 8; case Stage::WriteCopyByte:return 3+store; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ClearNotice:notice_=0;next();break;
case Stage::WriteClearedNotice:write(Write::Notice,0);next();break;
case Stage::WriteSelectedMode:write(Write::Mode,selector_);next();break;
case Stage::ReadNotice:notice_=uint8_t(read(Read::Notice,0));next();break;
case Stage::SelectNotice:stage_=notice_?Stage::ReadSelectedMode:Stage::SaveNoticeInput;break;
case Stage::SetNotice:notice_=1;next();break;
case Stage::WriteNotice:write(Write::Notice,notice_);next();break;
case Stage::ReadSelectedMode:mode_=uint8_t(read(Read::Mode,0));next();break;
case Stage::SelectModeFour:stage_=mode_==4?Stage::SaveModeInput:Stage::CompareModeFive;break;
case Stage::SelectModeFive:stage_=mode_==5?Stage::SaveModeInput:Stage::ReturnInactive;break;
case Stage::SelectPending:stage_=pending_?Stage::ShowInput:Stage::ReadEncoder;break;
case Stage::SelectFirstSelector:stage_=selector_==0x22?Stage::UpdateFirstRange:Stage::CompareSecondSelector;break;
case Stage::JoinFirst:stage_=Stage::ReadEncoder;break;
case Stage::SelectSecondSelector:stage_=selector_==0x23?Stage::UpdateSecondRange:Stage::CompareThirdSelector;break;
case Stage::JoinSecond:stage_=Stage::ReadEncoder;break;
case Stage::SelectThirdSelector:stage_=selector_==0x20?Stage::SaveThirdInput:Stage::ReadEncoder;break;
case Stage::SelectEncoder:stage_=delta_?Stage::ReadDuty:Stage::ReturnPoll;break;
case Stage::ReadDuty:duty_=uint8_t(read(Read::Duty,0));next();break;
case Stage::AddDuty:sum_=int(duty_)+int(int8_t(delta_));next();break;
case Stage::SelectDutySign:stage_=sum_<0?Stage::ClearDuty:Stage::CompareDutyLimit;break;
case Stage::ClearDuty:sum_=0;next();break;
case Stage::JoinDuty:stage_=Stage::WriteDuty;break;
case Stage::SelectDutyLimit:stage_=sum_<=255?Stage::WriteDuty:Stage::ClampDuty;break;
case Stage::ClampDuty:sum_=255;next();break;
case Stage::WriteDuty:write(Write::Duty,uint8_t(sum_));next();break;
case Stage::SaveCoefficients:stage_=Stage::BindSaveTarget;break;
case Stage::BindCopyCount:position_=0;next();break;
case Stage::ReadCopyByte:byte_=uint8_t(read(Read::CalibrationByte,position_));next();break;
case Stage::WriteCopyByte:write(Write::CalibrationByte,byte_);next();break;
case Stage::NextCopyTarget:++position_;next();break;
case Stage::RepeatCopy:stage_=position_<256?Stage::ReadCopyByte:Stage::BindHistoryInitial;break;
case Stage::WriteFirstHistory:write(Write::FirstHistory,0x60a0);next();break;
case Stage::WriteSecondHistory:write(Write::SecondHistory,0x60a0);next();break;
case Stage::ReadLoadedDuty:duty_=uint8_t(read(Read::Duty,0));next();break;
case Stage::LoadCalibration:await(Stage::ClearNotice);break;
case Stage::ReturnReset:stage_=Stage::Complete;break;
case Stage::ShowSelectedMode:await(Stage::ReadNotice);break;
case Stage::PrintNoticeText:await(Stage::RestoreNoticeInput);break;
case Stage::ReturnInactive:stage_=Stage::Complete;break;
case Stage::SetModePosition:await(Stage::PrintModeValue);break;
case Stage::PrintModeValue:await(Stage::RestoreModeInput);break;
case Stage::ReturnMode:stage_=Stage::Complete;break;
case Stage::PollInput:await(Stage::TestPending);break;
case Stage::ShowInput:await(Stage::CompareFirstSelector);break;
case Stage::UpdateFirstRange:await(Stage::SaveFirstInput);break;
case Stage::SetFirstPosition:await(Stage::RestoreFirstInput);break;
case Stage::ScaleFirst:await(Stage::BindFirstValue);break;
case Stage::DisplayFirstValue:await(Stage::JoinFirst);break;
case Stage::UpdateSecondRange:await(Stage::SaveSecondInput);break;
case Stage::SetSecondPosition:await(Stage::RestoreSecondInput);break;
case Stage::ScaleSecond:await(Stage::BindSecondValue);break;
case Stage::DisplaySecondValue:await(Stage::JoinSecond);break;
case Stage::SetThirdPosition:await(Stage::RestoreThirdInput);break;
case Stage::ScaleThird:await(Stage::BindThirdValue);break;
case Stage::DisplayThirdValue:await(Stage::ReadEncoder);break;
case Stage::ReadEncoder:await(Stage::TestEncoder);break;
case Stage::ApplyDuty:await(Stage::BindDutyPosition);break;
case Stage::SetDutyPosition:await(Stage::DisplayDuty);break;
case Stage::DisplayDuty:await(Stage::ReturnPoll);break;
case Stage::ReturnPoll:stage_=Stage::Complete;break;
case Stage::CalculateFirstCoefficients:await(Stage::BindSecondHistory);break;
case Stage::CalculateSecondCoefficients:await(Stage::SaveCoefficients);break;
case Stage::UpdateFirstHistory:await(Stage::BindFirstOutput);break;
case Stage::CalculateFirstOutput:await(Stage::ReturnFirst);break;
case Stage::ReturnFirst:stage_=Stage::Complete;break;
case Stage::UpdateSecondHistory:await(Stage::RestoreInvertedInput);break;
case Stage::CalculateSecondOutput:await(Stage::ReturnSecond);break;
case Stage::ReturnSecond:stage_=Stage::Complete;break;
case Stage::ApplyLoadedDuty:await(Stage::ReturnLoad);break;
case Stage::ReturnLoad:stage_=Stage::Complete;break;
case Stage::SaveCalibration:await(Stage::ReturnSave);break;
case Stage::ReturnSave:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint8_t selector_=0,notice_=0,mode_=0,duty_=0,byte_=0,delta_=0;unsigned position_=0;int sum_=0;bool pending_=false;
};
}
