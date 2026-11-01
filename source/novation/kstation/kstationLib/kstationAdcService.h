#pragma once
#include <cstdint>

namespace kstation {
// K ADC sample child.  The four ADC bytes/old words are read at their actual
// boundaries; in particular the group can alias the sample storage.  This is
// only the 06ba child, not the complete interrupt service.
class AdcSampleTask final {
public:
 enum class Stage { ClearGroup, ReadGroup, ScaleGroup, SelectSamples, SelectAdc,
  SetCount, ReadHigh, ReadLow, ReadPrevious, SetRawWeight, WeightRaw,
  BindRawHalf, SetPreviousWeight, WeightPrevious, AddHalves, SelectSaturation,
  Saturate, WriteSample, AdvanceSample, DecrementCount, MoreChannels, Return,
  Complete };
 enum class Read { Group, High, Low, Previous };
 struct Credits { unsigned read=0, write=0, returnStack=0; };
 Stage stage() const { return stage_; }
 bool complete() const { return stage_==Stage::Complete; }
 uint8_t group() const { return group_; }
 unsigned channel() const { return channel_; }
 uint32_t sampleAddress() const { return 0xc410u+uint8_t(group_<<1)+2*channel_; }
 uint32_t adcAddress() const { return 0xffffe0u+2*channel_; }
 uint16_t raw() const { return uint16_t((unsigned(high_)<<8)|low_); }
 uint16_t previous() const { return previous_; }
 uint16_t value() const { return uint16_t(sum_); }
 unsigned nextClocks(const Credits& c) const {
  switch(stage_) {
  case Stage::ReadGroup:return 9+c.read;
  case Stage::ReadHigh:case Stage::ReadLow:return 5+c.read;
  case Stage::ReadPrevious:return 3+c.read;
  case Stage::WriteSample:return 3+c.write;
  case Stage::SelectSamples:case Stage::SelectAdc:return 9;
  case Stage::SetRawWeight:case Stage::SetPreviousWeight:case Stage::Saturate:return 6;
  case Stage::WeightRaw:case Stage::WeightPrevious:return 23;
  case Stage::Return:return c.returnStack;
  case Stage::Complete:return 0;
  default:return 3;
  }
 }
 template<class Reader,class Writer>
 unsigned advanceBefore(unsigned available,const Credits& credits,Reader&& read,Writer&& write) {
  const auto cost=nextClocks(credits);if(!cost||cost>available)return 0;
  switch(stage_) {
  case Stage::ReadGroup:group_=uint8_t(read(Read::Group));next();break;
  case Stage::ReadHigh:high_=uint8_t(read(Read::High));next();break;
  case Stage::ReadLow:low_=uint8_t(read(Read::Low));next();break;
  case Stage::ReadPrevious:previous_=uint16_t(read(Read::Previous));next();break;
  case Stage::WeightRaw:rawHalf_=uint16_t(raw()>>1);next();break;
  case Stage::WeightPrevious:previousHalf_=uint16_t(previous_>>1);next();break;
  case Stage::AddHalves:sum_=unsigned(rawHalf_)+previousHalf_;next();break;
  case Stage::SelectSaturation:stage_=sum_>65535?Stage::Saturate:Stage::WriteSample;break;
  case Stage::Saturate:sum_=65535;next();break;
  case Stage::WriteSample:write(uint16_t(sum_));next();break;
  case Stage::AdvanceSample:++channel_;next();break;
  case Stage::MoreChannels:stage_=channel_<4?Stage::ReadHigh:Stage::Return;break;
  case Stage::Return:stage_=Stage::Complete;break;
  case Stage::Complete:break;
  default:next();break;
  }
  return cost;
 }
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}
 Stage stage_=Stage::ClearGroup;uint8_t group_=0,high_=0,low_=0;unsigned channel_=0;
 uint16_t previous_=0,rawHalf_=0,previousHalf_=0;unsigned sum_=0;
};
// Interrupt scan and event queue semantic continuation. The sample child is
// separately owned so that the ADC peripheral stays live at each transfer.
class AdcInterruptTask final {
public:
 enum class Stage { RetainBase, ReadRemaining, SelectScan, RetainRemaining, DecrementRemaining,
  WriteRemaining, ReadBusy, MaskBusy, WriteBusy, CompareRemaining, SelectSettle,
  RestoreQuick, FinishQuick, RetainShortHigh, RetainShortFirst, RetainShortSecond,
  RetainShortCount, RetainShortFourth, DispatchShortSample, RestoreShortFourth,
  RestoreShortCount, RestoreShortSecond, RestoreShortFirst, RestoreShortHigh,
  RestoreShortBase, FinishShort, ReadStatus, MaskStatus, WriteStatus, RetainLongHigh,
  RetainLongFirst, RetainLongSecond, RetainLongCount, RetainLongFourth, DispatchLongSample,
  RestoreLongFourth, ClearGroup, ReadGroup, CaptureChannel, ScaleGroup, LocateHistory,
  SetCount, ReadCurrent, ReadHistory, SetThreshold, ReadArmed, SelectThreshold,
  SetArmedThreshold, Difference, SelectStable, SelectDirection, NegateDifference,
  CompareNegative, SelectNegative, AdjustNegative, SkipPositive, ComparePositive,
  SelectPositive, AdjustPositive, ClearArmed, WriteArmed, WriteHistory, RetainAccepted,
  ReadScaleGroup, NormalizeScaleGroup, RetainScaleGroup, RestoreAccepted, CompareSpecial,
  SelectSpecial, SetSpecialFactor, MultiplySpecial, ExtractSpecial, ClearSpecialHigh,
  DoubleSpecial, BiasSpecial, SelectSpecialFloor, ClearSpecialFloor, CompareSpecialCeiling,
  SelectSpecialCeiling, ClampSpecial, SkipNormal, SetNormalFactor, MultiplyNormal,
  ExtractNormal, ClearNormalHigh, DoubleNormal, BiasNormal, SelectNormalFloor,
  ClearNormalFloor, CompareNormalCeiling, SelectNormalCeiling, ClampNormal, BindEvent,
  DispatchEvent, RetainEventFirst, RetainEventSecond, CopyEventIndex, NormalizeEventWord,
  NormalizeEventLong, LocateMapping, ReadMapping, SelectMapping, ReadProducer,
  NormalizeProducer, LocateQueue, LocateQueueEntry, WriteEvent, AdvanceProducer,
  MaskProducer, WriteProducer, RestoreEventSecond, RestoreEventFirst, ReturnEvent,
  AdvanceWord, AdvanceChannel, DecrementCount, MoreChannels, ReadNextGroup,
  IncrementGroup, WriteNextGroup, CompareNextGroup, SelectNextGroup, DispatchMux,
  ReadMuxGroup, ShiftMuxFirst, ShiftMuxSecond, WriteMux, BindSettleCount, WriteSettleCount,
  ReturnMux, SetConversion, WriteConversion, RestoreLongCount, RestoreLongSecond,
  RestoreLongFirst, RestoreLongHigh, RestoreLongBase, FinishLong, AwaitSample, Complete };
 using S=Stage;
 enum class Read { Remaining, Status, Group, Current, History, Armed, ScaleGroup,
                   Mapping, Producer, NextGroup, MuxGroup };
 enum class Write { Remaining, Status, Armed, History, Event, Producer, Group, Mux, SettleCount };
 struct Credits { unsigned read=0,write=0,stack=0; };
 S stage()const{return stage_;}bool complete()const{return stage_==S::Complete;}
 bool awaitingSample()const{return stage_==S::AwaitSample;}
 bool resumeSample(){if(!awaitingSample())return false;stage_=resume_;return true;}
 unsigned offset()const{return unsigned(uint8_t(group_<<1))+position_*2;}
 uint32_t historyAddress()const{return 0xc3d0u+offset();}
 uint32_t currentAddress()const{return historyAddress()+0x40;}
 uint32_t armedAddress()const{return historyAddress()+0x80;}
 uint32_t mappingAddress()const{return 0x84d2u+channel_;}
 uint32_t eventAddress()const{return 0xc1d0u+producer_;}
 uint8_t channel()const{return channel_;}uint16_t current()const{return current_;}
 uint16_t history()const{return history_;}uint16_t accepted()const{return accepted_;}
 uint16_t event()const{return uint16_t((scaled_&0xff00)|mapping_);}
 // Fetch and bus credits are supplied per phase by the execution host. The
 // stack credit includes fetch and return costs; no CPU state enters this task.
 unsigned nextClocks(const Credits& c)const {
  switch(stage_){
  case S::AwaitSample:case S::Complete:return 0;
  case S::RetainBase:case S::RestoreQuick:case S::FinishQuick:case S::RetainShortHigh:
  case S::RetainShortFirst:case S::RetainShortSecond:case S::RetainShortCount:case S::RetainShortFourth:
  case S::DispatchShortSample:case S::RestoreShortFourth:case S::RestoreShortCount:case S::RestoreShortSecond:
  case S::RestoreShortFirst:case S::RestoreShortHigh:case S::RestoreShortBase:case S::FinishShort:
  case S::RetainLongHigh:case S::RetainLongFirst:case S::RetainLongSecond:case S::RetainLongCount:
  case S::RetainLongFourth:case S::DispatchLongSample:case S::RestoreLongFourth:case S::RetainAccepted:
  case S::RestoreAccepted:case S::DispatchEvent:case S::RetainEventFirst:case S::RetainEventSecond:
  case S::RestoreEventSecond:case S::RestoreEventFirst:case S::ReturnEvent:case S::DispatchMux:
  case S::ReturnMux:case S::RestoreLongCount:case S::RestoreLongSecond:case S::RestoreLongFirst:
  case S::RestoreLongHigh:case S::RestoreLongBase:case S::FinishLong:return c.stack;
  case S::ReadRemaining:case S::ReadGroup:case S::ReadScaleGroup:case S::ReadProducer:
  case S::ReadNextGroup:case S::ReadMuxGroup:return 9+c.read;
  case S::ReadBusy:case S::ReadStatus:case S::ReadMapping:case S::ReadHistory:return 3+c.read;
  case S::ReadCurrent:case S::ReadArmed:return 6+c.read;
  case S::WriteRemaining:case S::WriteProducer:case S::WriteNextGroup:case S::WriteSettleCount:return 9+c.write;
  case S::WriteBusy:case S::WriteStatus:case S::WriteConversion:case S::WriteMux:
  case S::WriteHistory:case S::WriteEvent:return 3+c.write;
  case S::WriteArmed:return 6+c.write;
  case S::SelectScan:case S::SelectStable:case S::SelectNegative:case S::MoreChannels:return 8;
  case S::LocateHistory:case S::BiasSpecial:case S::CompareSpecialCeiling:case S::BiasNormal:
  case S::CompareNormalCeiling:case S::LocateMapping:case S::LocateQueue:return 9;
  case S::SetThreshold:case S::SetArmedThreshold:case S::AdjustNegative:case S::AdjustPositive:
  case S::CompareSpecial:case S::SetSpecialFactor:case S::SetNormalFactor:case S::MaskProducer:return 6;
  case S::MultiplySpecial:case S::MultiplyNormal:return 23;
  default:return 3;
  }
 }
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits& c,Reader&&read,Writer&&write){
  const auto cost=nextClocks(c);if(!cost||cost>available)return 0;
  switch(stage_){
  case S::ReadRemaining:remaining_=uint8_t(read(Read::Remaining));next();break;
  case S::SelectScan:stage_=remaining_?S::RetainRemaining:S::ReadStatus;break;
  case S::DecrementRemaining:--remaining_;next();break;
  case S::WriteRemaining:write(Write::Remaining,remaining_);next();break;
  case S::ReadBusy:case S::ReadStatus:status_=uint8_t(read(Read::Status));next();break;
  case S::MaskBusy:status_&=0x7f;next();break;
  case S::MaskStatus:status_&=0x5f;next();break;
  case S::WriteBusy:case S::WriteStatus:write(Write::Status,status_);next();break;
  case S::SelectSettle:stage_=remaining_?S::RestoreQuick:S::RetainShortHigh;break;
  case S::DispatchShortSample:resume_=S::RestoreShortFourth;stage_=S::AwaitSample;break;
  case S::DispatchLongSample:resume_=S::RestoreLongFourth;stage_=S::AwaitSample;break;
  case S::ReadGroup:group_=uint8_t(read(Read::Group));next();break;
  case S::CaptureChannel:channel_=group_;next();break;
  case S::ReadCurrent:current_=uint16_t(read(Read::Current));next();break;
  case S::ReadHistory:history_=uint16_t(read(Read::History));next();break;
  case S::SetThreshold:threshold_=0x200;next();break;
  case S::ReadArmed:armed_=uint8_t(read(Read::Armed));next();break;
  case S::SelectThreshold:stage_=armed_?S::SetArmedThreshold:S::Difference;break;
  case S::SetArmedThreshold:threshold_=0x800;next();break;
  case S::Difference:difference_=uint16_t(history_-current_);next();break;
  case S::SelectStable:stage_=difference_?S::SelectDirection:S::AdvanceWord;break;
  case S::SelectDirection:stage_=(difference_&0x8000)?S::NegateDifference:S::ComparePositive;break;
  case S::NegateDifference:difference_=uint16_t(0-difference_);next();break;
  case S::SelectNegative:stage_=difference_<threshold_?S::AdvanceWord:S::AdjustNegative;break;
  case S::AdjustNegative:accepted_=uint16_t(current_-0x100);next();break;
  case S::SkipPositive:stage_=S::ClearArmed;break;
  case S::SelectPositive:stage_=difference_<threshold_?S::AdvanceWord:S::AdjustPositive;break;
  case S::AdjustPositive:accepted_=uint16_t(current_+0x100);next();break;
  case S::WriteArmed:write(Write::Armed,0);next();break;
  case S::WriteHistory:write(Write::History,accepted_);next();break;
  case S::ReadScaleGroup:scaleGroup_=uint8_t(read(Read::ScaleGroup));next();break;
  case S::SelectSpecial:stage_=scaleGroup_==28?S::SetSpecialFactor:S::SetNormalFactor;break;
  case S::MultiplySpecial:scaled_=(unsigned(accepted_)*0x8207)>>16;next();break;
  case S::MultiplyNormal:scaled_=(unsigned(accepted_)*0x4144)>>16;next();break;
  case S::DoubleSpecial:case S::DoubleNormal:scaled_*=2;next();break;
  case S::BiasSpecial:underflow_=scaled_<0x200;scaled_=uint32_t(scaled_-0x200);next();break;
  case S::SelectSpecialFloor:stage_=underflow_?S::ClearSpecialFloor:S::CompareSpecialCeiling;break;
  case S::ClearSpecialFloor:scaled_=0;next();break;
  case S::SelectSpecialCeiling:stage_=scaled_<=0xff00?S::SkipNormal:S::ClampSpecial;break;
  case S::ClampSpecial:scaled_=(scaled_&255)|0xff00;next();break;
  case S::SkipNormal:stage_=S::BindEvent;break;
  case S::BiasNormal:underflow_=scaled_<0x100;scaled_=uint32_t(scaled_-0x100);next();break;
  case S::SelectNormalFloor:stage_=underflow_?S::ClearNormalFloor:S::CompareNormalCeiling;break;
  case S::ClearNormalFloor:scaled_=0;next();break;
  case S::SelectNormalCeiling:stage_=scaled_<=0x7f00?S::BindEvent:S::ClampNormal;break;
  case S::ClampNormal:scaled_=(scaled_&255)|0x7f00;next();break;
  case S::ReadMapping:mapping_=uint8_t(read(Read::Mapping));next();break;
  case S::SelectMapping:stage_=(mapping_&128)?S::RestoreEventSecond:S::ReadProducer;break;
  case S::ReadProducer:producer_=uint16_t(read(Read::Producer));next();break;
  case S::WriteEvent:write(Write::Event,event());next();break;
  case S::AdvanceProducer:producer_=uint16_t(producer_+2);next();break;
  case S::MaskProducer:producer_&=0x1fe;next();break;
  case S::WriteProducer:write(Write::Producer,producer_);next();break;
  case S::AdvanceWord:++position_;next();break;
  case S::AdvanceChannel:++channel_;next();break;
  case S::MoreChannels:stage_=position_<4?S::ReadCurrent:S::ReadNextGroup;break;
  case S::ReadNextGroup:nextGroup_=uint8_t(read(Read::NextGroup));next();break;
  case S::IncrementGroup:nextGroup_=uint8_t(nextGroup_+4);next();break;
  case S::WriteNextGroup:write(Write::Group,nextGroup_);next();break;
  case S::SelectNextGroup:stage_=nextGroup_<=28?S::DispatchMux:S::RestoreLongCount;break;
  case S::ReadMuxGroup:mux_=uint8_t(read(Read::MuxGroup));next();break;
  case S::ShiftMuxFirst:case S::ShiftMuxSecond:mux_>>=1;next();break;
  case S::WriteMux:write(Write::Mux,mux_);next();break;
  case S::WriteSettleCount:write(Write::SettleCount,9);next();break;
  case S::WriteConversion:write(Write::Status,0x73);next();break;
  case S::FinishQuick:case S::FinishShort:case S::FinishLong:stage_=S::Complete;break;
  case S::AwaitSample:case S::Complete:break;
  default:next();break;
  }return cost;
 }
private:
 void next(){stage_=S(unsigned(stage_)+1);}
 S stage_=S::RetainBase,resume_=S::Complete;unsigned position_=0;
 uint8_t remaining_=0,status_=0,group_=0,channel_=0,armed_=0,scaleGroup_=0,mapping_=0,nextGroup_=0,mux_=0;
 uint16_t current_=0,history_=0,threshold_=0,difference_=0,accepted_=0,producer_=0;
 uint32_t scaled_=0;bool underflow_=false;
};
// Shared 065a mux/start helper, also called from foreground 0638.
class AdcMuxTask final {
public:
 enum class Stage { ReadGroup,ShiftFirst,ShiftSecond,WriteMux,BindCount,WriteCount,Return,Complete };
 enum class Write { Mux,Count };
 struct Credits {unsigned read=0,write=0,returnStack=0;};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}uint8_t mux()const{return mux_;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::ReadGroup:return 9+c.read;case Stage::WriteMux:return 3+c.write;case Stage::WriteCount:return 9+c.write;case Stage::Return:return c.returnStack;case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read,Writer&&write){auto cost=nextClocks(c);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadGroup:mux_=uint8_t(read());stage_=Stage::ShiftFirst;break;case Stage::ShiftFirst:case Stage::ShiftSecond:mux_>>=1;stage_=Stage(unsigned(stage_)+1);break;case Stage::WriteMux:write(Write::Mux,mux_);stage_=Stage::BindCount;break;case Stage::WriteCount:write(Write::Count,9);stage_=Stage::Return;break;case Stage::Return:stage_=Stage::Complete;break;case Stage::Complete:break;default:stage_=Stage(unsigned(stage_)+1);break;}return cost;}
private:Stage stage_=Stage::ReadGroup;uint8_t mux_=0;
};

}
