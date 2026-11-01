#pragma once
#include <cstdint>
namespace ks {
// ADC completion owns settling, four ordered threshold decisions, retained
// history, changed-channel preview and the next conversion decision. Children
// keep original observation/IRQ boundaries; no atomic scan or host-time polling.
class PanelAdcIrqTask final {
public:
 enum class Stage { SaveBase, ReadRemaining, SelectScan, SaveRemaining, DecrementRemaining, WriteRemaining, ReadBusy, MaskBusy, WriteBusy, CompareRemaining, SelectSettle, RestoreQuick, ReturnQuick, SaveShortHigh, SaveShortFirst, SaveShortSecond, SaveShortCount, SaveShortFourth, SampleSettle, RestoreShortFourth, RestoreShortCount, RestoreShortSecond, RestoreShortFirst, RestoreShortHigh, RestoreShortBase, ReturnSettle, ReadStatus, MaskStatus, WriteStatus, SaveLongHigh, SaveLongFirst, SaveLongSecond, SaveLongCount, SaveLongFourth, SampleScan, ClearGroup, ReadGroup, CaptureChannel, DoubleGroup, LocateHistory, ClearTableIndex, CaptureTableIndex, LocateTable, SetChannels, ReadCurrent, ReadHistory, SetThreshold, ReadArmed, SelectThreshold, SetArmedThreshold, Difference, SelectStable, SelectDirection, NegateDifference, CompareNegative, AdmitNegative, AdjustNegative, SkipPositive, ComparePositive, AdmitPositive, AdjustPositive, ClearArmed, WriteArmed, ReadPreview, WriteHistory, SaveAccepted, ReadScaleGroup, MaskScaleGroup, RetainScaleGroup, RestoreAccepted, CompareSpecial, SelectSpecial, ReadLast, CompareLast, SelectLast, SavePreview, BindPreview, ScalePreview, RestorePreview, ScaleAccepted, SkipEvent, AdvanceWord, AdvanceTable, AdvanceChannel, DecrementChannels, MoreChannels, ReadNextGroup, IncrementGroup, WriteNextGroup, CompareNextGroup, SelectNextGroup, SelectMux, SetConversion, WriteConversion, RestoreLongFourth, RestoreLongCount, RestoreLongSecond, RestoreLongFirst, RestoreLongHigh, RestoreLongBase, ReturnScan, AwaitChild, Complete };
 enum class Read { Remaining, Status, Group, Current, History, Armed, Preview, ScaleGroup, Last, NextGroup };
 enum class Write { Remaining, Status, Armed, History, Group };
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}
 bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 uint8_t remaining()const{return remaining_;}uint8_t status()const{return status_;}uint8_t group()const{return group_;}
 unsigned offset()const{return unsigned(uint8_t(group_<<1))+position_*2;}
 uint8_t channel()const{return channel_;}uint16_t current()const{return current_;}uint16_t history()const{return history_;}
 uint8_t armed()const{return armed_;}uint16_t preview()const{return preview_;}uint8_t scaleGroup()const{return scaleGroup_;}
 uint8_t last()const{return last_;}uint8_t nextGroup()const{return nextGroup_;}uint16_t accepted()const{return accepted_;}
 unsigned nextClocks(unsigned irqReturn=23,unsigned countWrite=3)const{switch(stage_){
 case Stage::ReadBusy:case Stage::WriteBusy:case Stage::ReadStatus:case Stage::WriteStatus:case Stage::WriteConversion:return 4;
 case Stage::ReadHistory:case Stage::SetThreshold:case Stage::SetArmedThreshold:case Stage::AdjustNegative:case Stage::AdjustPositive:case Stage::ReadPreview:case Stage::WriteHistory:case Stage::CompareSpecial:return 6;
 case Stage::SelectScan:case Stage::SelectStable:case Stage::AdmitNegative:case Stage::AdmitPositive:case Stage::MoreChannels:return 8;
 case Stage::LocateHistory:case Stage::LocateTable:case Stage::ReadCurrent:case Stage::ReadArmed:case Stage::WriteArmed:return 9;
 case Stage::SaveBase:case Stage::RestoreQuick:case Stage::SaveShortHigh:case Stage::SaveShortCount:case Stage::RestoreShortCount:case Stage::RestoreShortHigh:case Stage::RestoreShortBase:case Stage::SaveLongHigh:case Stage::SaveAccepted:case Stage::RestoreAccepted:case Stage::SavePreview:case Stage::RestorePreview:case Stage::RestoreLongHigh:case Stage::RestoreLongBase:return 11;
 case Stage::ReadRemaining:case Stage::ReadGroup:case Stage::ReadScaleGroup:case Stage::ReadLast:case Stage::ReadNextGroup:case Stage::WriteNextGroup:return 12;
 case Stage::SampleSettle:case Stage::SampleScan:case Stage::ScalePreview:case Stage::ScaleAccepted:return 15;
 case Stage::SaveShortFirst:case Stage::SaveShortSecond:case Stage::SaveShortFourth:case Stage::RestoreShortFourth:case Stage::RestoreShortSecond:case Stage::RestoreShortFirst:case Stage::SaveLongFirst:case Stage::SaveLongSecond:case Stage::SaveLongCount:case Stage::SaveLongFourth:case Stage::SelectMux:case Stage::RestoreLongFourth:case Stage::RestoreLongCount:case Stage::RestoreLongSecond:case Stage::RestoreLongFirst:return 20;
 case Stage::ReturnQuick:case Stage::ReturnSettle:case Stage::ReturnScan:return irqReturn;
 case Stage::WriteRemaining:return 9+countWrite;case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;
 }}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned irqReturn=23,unsigned countWrite=3){
  const auto cost=nextClocks(irqReturn,countWrite);if(!cost||cost>available)return 0;
  switch(stage_){
  case Stage::ReadRemaining:remaining_=uint8_t(read(Read::Remaining));next();break;
  case Stage::SelectScan:stage_=remaining_?Stage::SaveRemaining:Stage::ReadStatus;break;
  case Stage::DecrementRemaining:--remaining_;next();break;
  case Stage::WriteRemaining:write(Write::Remaining,remaining_);next();break;
  case Stage::ReadBusy:case Stage::ReadStatus:status_=uint8_t(read(Read::Status));next();break;
  case Stage::MaskBusy:status_&=127;next();break;case Stage::MaskStatus:status_&=95;next();break;
  case Stage::WriteBusy:case Stage::WriteStatus:write(Write::Status,status_);next();break;
  case Stage::SelectSettle:stage_=remaining_?Stage::RestoreQuick:Stage::SaveShortHigh;break;
  case Stage::SampleSettle:await(Stage::RestoreShortFourth);break;
  case Stage::SampleScan:await(Stage::ClearGroup);break;
  case Stage::ReadGroup:group_=uint8_t(read(Read::Group));next();break;
  case Stage::CaptureChannel:channel_=group_;next();break;
  case Stage::ReadCurrent:current_=uint16_t(read(Read::Current));next();break;
  case Stage::ReadHistory:history_=uint16_t(read(Read::History));next();break;
  case Stage::SetThreshold:threshold_=512;next();break;
  case Stage::ReadArmed:armed_=uint8_t(read(Read::Armed));next();break;
  case Stage::SelectThreshold:stage_=armed_?Stage::SetArmedThreshold:Stage::Difference;break;
  case Stage::SetArmedThreshold:threshold_=2048;next();break;
  case Stage::Difference:difference_=uint16_t(history_-current_);next();break;
  case Stage::SelectStable:stage_=difference_?Stage::SelectDirection:Stage::AdvanceWord;break;
  case Stage::SelectDirection:stage_=difference_&32768?Stage::NegateDifference:Stage::ComparePositive;break;
  case Stage::NegateDifference:difference_=uint16_t(0-difference_);next();break;
  case Stage::AdmitNegative:stage_=difference_<threshold_?Stage::AdvanceWord:Stage::AdjustNegative;break;
  case Stage::AdmitPositive:stage_=difference_<threshold_?Stage::AdvanceWord:Stage::AdjustPositive;break;
  case Stage::AdjustNegative:accepted_=uint16_t(current_-256);next();break;
  case Stage::SkipPositive:stage_=Stage::ClearArmed;break;
  case Stage::AdjustPositive:accepted_=uint16_t(current_+256);next();break;
  case Stage::ClearArmed:armed_=0;next();break;
  case Stage::WriteArmed:write(Write::Armed,0);next();break;
  case Stage::ReadPreview:preview_=uint16_t(read(Read::Preview));next();break;
  case Stage::WriteHistory:write(Write::History,accepted_);next();break;
  case Stage::ReadScaleGroup:scaleGroup_=uint8_t(read(Read::ScaleGroup));next();break;
  case Stage::SelectSpecial:if(scaleGroup_==32)await(Stage::AdvanceWord);else stage_=Stage::ReadLast;break;
  case Stage::ReadLast:last_=uint8_t(read(Read::Last));next();break;
  case Stage::SelectLast:stage_=channel_==last_?Stage::ScaleAccepted:Stage::SavePreview;break;
  case Stage::ScalePreview:await(Stage::RestorePreview);break;
  case Stage::ScaleAccepted:await(Stage::SkipEvent);break;
  case Stage::SkipEvent:stage_=Stage::AdvanceWord;break;
  case Stage::AdvanceWord:++position_;next();break;
  case Stage::AdvanceChannel:++channel_;next();break;
  case Stage::MoreChannels:stage_=position_<4?Stage::ReadCurrent:Stage::ReadNextGroup;break;
  case Stage::ReadNextGroup:nextGroup_=uint8_t(read(Read::NextGroup));next();break;
  case Stage::IncrementGroup:nextGroup_=uint8_t(nextGroup_+4);next();break;
  case Stage::WriteNextGroup:write(Write::Group,nextGroup_);next();break;
  case Stage::SelectNextGroup:stage_=nextGroup_<=32?Stage::SelectMux:Stage::RestoreLongFourth;break;
  case Stage::SelectMux:await(Stage::SetConversion);break;
  case Stage::WriteConversion:write(Write::Status,0x73);next();break;
  case Stage::ReturnQuick:case Stage::ReturnSettle:case Stage::ReturnScan:stage_=Stage::Complete;break;
  default:next();break;
  }return cost;
 }
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::SaveBase,resume_=Stage::Complete;unsigned position_=0;
 uint8_t remaining_=0,status_=0,group_=0,channel_=0,armed_=0,scaleGroup_=0,last_=0,nextGroup_=0;
 uint16_t current_=0,history_=0,threshold_=0,difference_=0,preview_=0,accepted_=0;
};
}
