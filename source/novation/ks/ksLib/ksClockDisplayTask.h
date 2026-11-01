#pragma once
#include <cstdint>
namespace ks {
// Clock status string selection and the original byte-only tempo rounding.
class ClockDisplayTask final {
public:
 enum class Kind { Display };enum class Stage { SaveDisplayValue, SaveDisplayIndex, SaveDisplayText, BindDisplayPosition, SetDisplayPosition, ReadActive, SelectActive, ReadFlags, TestFlags, SelectFlags, IncrementActive, ScaleTextIndex, ClearTextAddress, BindTextIndex, LocateText, DisplayText, BindNumberPosition, SetNumberPosition, ReadPeriod, BindPeriodWeight, DividePeriod, QuarterFirst, QuarterSecond, HalfNumber, SelectRound, RoundLowByte, DisplayNumber, RestoreDisplayText, RestoreDisplayIndex, RestoreDisplayValue, ReturnDisplay, AwaitChild, Complete };enum class Read { Active, Flags, Period };
 explicit ClockDisplayTask(Kind){}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 uint8_t index()const{return index_;}uint8_t flags()const{return flags_;}uint32_t period()const{return period_;}uint16_t number()const{return number_;}
 unsigned nextClocks()const{switch(stage_){case Stage::BindPeriodWeight:return 6; case Stage::LocateText:return 9; case Stage::ReadActive:case Stage::ReadFlags:return 12; case Stage::ReturnDisplay:return 17; case Stage::SaveDisplayValue:case Stage::SaveDisplayIndex:case Stage::SaveDisplayText:case Stage::SetDisplayPosition:case Stage::DisplayText:case Stage::SetNumberPosition:case Stage::DisplayNumber:case Stage::RestoreDisplayText:case Stage::RestoreDisplayIndex:case Stage::RestoreDisplayValue:return 20; case Stage::DividePeriod:return 23; case Stage::ReadPeriod:return 24; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){const auto cost=nextClocks();if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadActive:index_=uint8_t(read(Read::Active));next();break;
case Stage::SelectActive:stage_=index_?Stage::ReadFlags:Stage::ScaleTextIndex;break;
case Stage::ReadFlags:flags_=uint8_t(read(Read::Flags));next();break;
case Stage::SelectFlags:stage_=flags_&1?Stage::ScaleTextIndex:Stage::IncrementActive;break;
case Stage::IncrementActive:++index_;next();break;
case Stage::ScaleTextIndex:index_=uint8_t(index_<<2);next();break;
case Stage::ReadPeriod:period_=read(Read::Period);next();break;
case Stage::DividePeriod:number_=uint16_t(period_/0xd1b7u);next();break;
case Stage::QuarterFirst:number_>>=2;next();break;
case Stage::QuarterSecond:number_>>=2;next();break;
case Stage::HalfNumber:round_=number_&1;number_>>=1;next();break;
case Stage::SelectRound:stage_=round_?Stage::RoundLowByte:Stage::DisplayNumber;break;
case Stage::RoundLowByte:number_=uint16_t((number_&0xff00)|uint8_t(number_+1));next();break;
case Stage::ReturnDisplay:stage_=Stage::Complete;break;
case Stage::SetDisplayPosition:await(Stage::ReadActive);break;
case Stage::DisplayText:await(Stage::BindNumberPosition);break;
case Stage::SetNumberPosition:await(Stage::ReadPeriod);break;
case Stage::DisplayNumber:await(Stage::RestoreDisplayText);break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::SaveDisplayValue,resume_=Stage::Complete;uint8_t index_=0,flags_=0;uint32_t period_=0;uint16_t number_=0;bool round_=false;
};
}
