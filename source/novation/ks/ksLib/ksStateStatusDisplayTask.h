#pragma once
#include <cstdint>
namespace ks {
// StateStatusDisplay semantic policy: logical data, decisions and deferred effects; compatibility frame stays in Calls.
class StateStatusDisplayTask final {
public:
 enum class Kind { Tick, Part, Compare, Schedule };enum class Stage { ReadNoticeBusy, ReadPageBusy, JoinPageBusy, ReadLatchBusy, JoinLatchBusy, ReadUiBusy, JoinUiBusy, SelectBusy, ReadSpecialBusy, SelectSpecialBusy, ReadSpecialCharacter, ShowSpecialCharacter, JoinSpecial, ReadStatusGate, SelectStatusGate, ReadStatusScreen, CompareStatusScreen, RejectStatusScreen, ReadStatusPage, CompareMeterPage, SelectMeterPage, ShowMeter, JoinMeter, CompareInputPage, SelectInputPage, CompareOutputPage, RejectOutputPage, BindMeterInput, ReadMeterInput, BindMeterRange, MultiplyMeterInput, BindMeterHigh, BiasMeterHigh, SelectMeterFloor, ClearMeterFloor, HalveMeterValue, BindMeterValue, ShowMeterValue, BindFastPeriod, JoinFastPeriod, ShowPartStatus, BindNormalPeriod, BindStatusCallback, ScheduleStatus, ReturnStatus, ReadPartMode, SelectPartMode, BindSinglePosition, ShowSinglePosition, BindSingleMarker, JoinSingleMarker, BindScanAttempts, ReadScanPart, NextScanPart, MaskScanPart, WriteScanPart, BindScanPosition, BindScanStride, ScaleScanPosition, BiasScanPosition, ShowScanPosition, BindInactiveMarker, ReadActiveMask, TestActiveMask, SelectActiveMask, ShowInactiveMarker, TickScanAttempts, RepeatScanPart, ReadInactivePartMarker, ShowInactivePartMarker, ReturnInactive, ReadSavedProgram, SaveSavedProgram, ClearScanIndex, BindScanIndex, DoubleScanIndex, ReadScanProgram, WriteScanProgram, ReadSavedPatch, SaveSavedPatch, NormalizeScanIndex, BindScanPatchSize, ScaleScanPatch, LocateScanPatch, WriteScanPatch, CompareScanPatch, RestoreSavedPatch, WriteSavedPatch, RestoreSavedProgram, WriteSavedProgram, ReturnActive, ReadCompareProgram, CompareStandardBank, SelectStandardBank, ResolveStandardPatch, ReadStandardPatch, BindStandardLength, ReadStandardSource, ReadStandardTarget, CompareStandardByte, SelectStandardMismatch, TickStandardLength, RepeatStandard, SelectStandardMatch, JoinStandardMismatch, CompareFxProgram, SelectFxProgram, ResolveSplitPatch, ReadSplitPatch, BindPrefixLength, ReadPrefixSource, ReadPrefixTarget, ComparePrefixByte, SelectPrefixMismatch, TickPrefixLength, RepeatPrefix, SelectPrefixMatch, SkipSourcePrefix, SkipTargetPrefix, ReadHeaderSource, ReadHeaderTarget, CompareHeaderByte, SelectHeaderMismatch, SkipSourceHeader, SkipTargetHeader, BindSuffixLength, ReadSuffixSource, ReadSuffixTarget, CompareSuffixByte, SelectSuffixMismatch, TickSuffixLength, RepeatSuffix, SelectSuffixMatch, JoinSplitMatch, ResolveFxPatch, ReadFxPatch, SkipSourceFx, SkipTargetFx, BindFxLength, ReadFxSource, ReadFxTarget, CompareFxByte, SelectFxMismatch, TickFxLength, RepeatFx, SelectFxMatch, SkipTargetTail, SkipSourceTail, ReadTailSource, ReadTailTarget, CompareTailByte, SelectTailMatch, BindChangedMarker, ShowChangedMarker, ReadCompareMarker, ShowCompareMarker, ReturnCompare, AwaitChild, Complete };
 enum class Exit { None, Compare };Exit exit()const{return exit_;}enum class Read { Notice, Page, Latch, Ui, Special, SpecialCharacter, Screen, EditPage, Mode, ScanPart, Mask, Marker, Program, PartProgram, Patch, Byte };enum class Write { ScanPart, Program, Patch };uint32_t busy()const{return busy_;}uint32_t value()const{return value_;}uint32_t page()const{return page_;}uint32_t index()const{return index_;}uint32_t mask()const{return mask_;}uint32_t savedProgram()const{return savedProgram_;}uint32_t program()const{return program_;}uint32_t savedPatch()const{return savedPatch_;}uint32_t target()const{return target_;}uint32_t sourceByte()const{return sourceByte_;}uint32_t targetByte()const{return targetByte_;}
 explicit StateStatusDisplayTask(Kind kind){switch(kind){case Kind::Tick:stage_=Stage::ReadNoticeBusy;break;case Kind::Part:stage_=Stage::ReadPartMode;break;case Kind::Compare:stage_=Stage::ReadCompareProgram;break;case Kind::Schedule:stage_=Stage::BindNormalPeriod;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(bool gate,uint16_t raw,uint32_t source){if(!awaitingChild())return false;if(resume_==Stage::SelectStatusGate)gate_=gate;if(resume_==Stage::BindMeterRange)raw_=raw;if(resume_==Stage::ReadStandardPatch||resume_==Stage::ReadSplitPatch||resume_==Stage::ReadFxPatch)source_=source;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ReadNoticeBusy:case Stage::ReadPageBusy:case Stage::ReadLatchBusy:case Stage::ReadUiBusy:case Stage::ReadSpecialBusy:case Stage::ReadSpecialCharacter:case Stage::ReadStatusScreen:case Stage::ReadStatusPage:case Stage::ReadPartMode:case Stage::ReadScanPart:case Stage::WriteScanPart:case Stage::ReadActiveMask:case Stage::ReadInactivePartMarker:case Stage::ReadSavedProgram:case Stage::WriteScanProgram:case Stage::WriteSavedProgram:case Stage::ReadCompareProgram:case Stage::ReadCompareMarker:return 9+store; case Stage::ShowSpecialCharacter:case Stage::ReadStatusGate:case Stage::ShowMeter:case Stage::ReadMeterInput:case Stage::ShowMeterValue:case Stage::ScheduleStatus:case Stage::ShowSinglePosition:case Stage::ShowScanPosition:case Stage::ShowInactiveMarker:case Stage::ShowInactivePartMarker:case Stage::SaveSavedPatch:case Stage::RestoreSavedPatch:case Stage::ResolveStandardPatch:case Stage::ResolveSplitPatch:case Stage::ResolveFxPatch:case Stage::ShowChangedMarker:case Stage::ShowCompareMarker:return 20; case Stage::BindMeterInput:case Stage::BindMeterRange:case Stage::BiasMeterHigh:case Stage::BindFastPeriod:case Stage::BindNormalPeriod:case Stage::BindScanPatchSize:case Stage::BindStandardLength:case Stage::BindPrefixLength:case Stage::BindSuffixLength:case Stage::BindFxLength:return 6; case Stage::MultiplyMeterInput:case Stage::ScaleScanPatch:return 23; case Stage::ShowPartStatus:case Stage::ScaleScanPosition:case Stage::CompareScanPatch:return 15; case Stage::BindStatusCallback:case Stage::LocateScanPatch:case Stage::SkipSourcePrefix:case Stage::SkipTargetPrefix:case Stage::SkipSourceFx:case Stage::SkipTargetFx:return 9; case Stage::ReturnStatus:case Stage::ReturnInactive:case Stage::ReturnActive:case Stage::ReturnCompare:return 17; case Stage::JoinSingleMarker:case Stage::SelectStandardMatch:case Stage::JoinStandardMismatch:return 8; case Stage::SaveSavedProgram:case Stage::RestoreSavedProgram:return 11; case Stage::ReadScanProgram:case Stage::ReadSavedPatch:case Stage::WriteScanPatch:case Stage::WriteSavedPatch:case Stage::ReadStandardPatch:case Stage::ReadSplitPatch:case Stage::ReadFxPatch:return 12+store; case Stage::ReadStandardSource:case Stage::ReadStandardTarget:case Stage::ReadPrefixSource:case Stage::ReadPrefixTarget:case Stage::ReadSuffixSource:case Stage::ReadSuffixTarget:case Stage::ReadFxSource:case Stage::ReadFxTarget:return 5+store; case Stage::ReadHeaderSource:case Stage::ReadHeaderTarget:case Stage::ReadTailSource:case Stage::ReadTailTarget:return 3+store; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadNoticeBusy:busy_=uint8_t(read(Read::Notice,0));next();break;
case Stage::ReadPageBusy:value_=uint8_t(read(Read::Page,0));next();break;
case Stage::JoinPageBusy:busy_|=value_;next();break;
case Stage::ReadLatchBusy:value_=uint8_t(read(Read::Latch,0));next();break;
case Stage::JoinLatchBusy:busy_|=value_;next();break;
case Stage::ReadUiBusy:value_=uint8_t(read(Read::Ui,0));next();break;
case Stage::JoinUiBusy:busy_|=value_;next();break;
case Stage::SelectBusy:stage_=busy_?Stage::BindNormalPeriod:Stage::ReadSpecialBusy;break;
case Stage::ReadSpecialBusy:value_=uint8_t(read(Read::Special,0));next();break;
case Stage::SelectSpecialBusy:stage_=value_?Stage::ReadSpecialCharacter:Stage::ReadStatusGate;break;
case Stage::ReadSpecialCharacter:value_=uint8_t(read(Read::SpecialCharacter,0));next();break;
case Stage::SelectStatusGate:stage_=gate_?Stage::ReadStatusScreen:Stage::ShowPartStatus;break;
case Stage::ReadStatusScreen:value_=uint8_t(read(Read::Screen,0));next();break;
case Stage::RejectStatusScreen:stage_=value_?Stage::BindNormalPeriod:Stage::ReadStatusPage;break;
case Stage::ReadStatusPage:page_=uint8_t(read(Read::EditPage,0));next();break;
case Stage::SelectMeterPage:stage_=page_==5?Stage::ShowMeter:Stage::CompareInputPage;break;
case Stage::SelectInputPage:stage_=page_==11?Stage::BindMeterInput:Stage::CompareOutputPage;break;
case Stage::RejectOutputPage:stage_=page_==12?Stage::BindMeterInput:Stage::BindNormalPeriod;break;
case Stage::MultiplyMeterInput:meter_=uint16_t(uint32_t(raw_)*768>>16);next();break;
case Stage::BiasMeterHigh:meter_=uint16_t(meter_-717);next();break;
case Stage::SelectMeterFloor:stage_=meter_&0x8000?Stage::ClearMeterFloor:Stage::HalveMeterValue;break;
case Stage::ClearMeterFloor:meter_&=0xff00;next();break;
case Stage::HalveMeterValue:value_=uint8_t(meter_)>>1;next();break;
case Stage::ReadPartMode:value_=uint8_t(read(Read::Mode,0));next();break;
case Stage::SelectPartMode:stage_=value_?Stage::BindScanAttempts:Stage::BindSinglePosition;break;
case Stage::BindScanAttempts:remaining_=3;next();break;
case Stage::ReadScanPart:index_=uint8_t(read(Read::ScanPart,0));next();break;
case Stage::NextScanPart:++index_;next();break;
case Stage::MaskScanPart:index_&=3;next();break;
case Stage::WriteScanPart:write(Write::ScanPart,index_);next();break;
case Stage::ReadActiveMask:mask_=uint8_t(read(Read::Mask,0));next();break;
case Stage::SelectActiveMask:stage_=mask_&(1u<<(index_&7))?Stage::ReadSavedProgram:Stage::ShowInactiveMarker;break;
case Stage::TickScanAttempts:--remaining_;next();break;
case Stage::RepeatScanPart:stage_=remaining_?Stage::ReadScanPart:Stage::ReadInactivePartMarker;break;
case Stage::ReadInactivePartMarker:value_=uint8_t(read(Read::Marker,0));next();break;
case Stage::ReadSavedProgram:savedProgram_=uint16_t(read(Read::Program,0));next();break;
case Stage::ReadScanProgram:program_=uint16_t(read(Read::PartProgram,92080+uint8_t(index_<<1)));next();break;
case Stage::WriteScanProgram:write(Write::Program,program_);next();break;
case Stage::ReadSavedPatch:savedPatch_=read(Read::Patch,0);next();break;
case Stage::WriteScanPatch:write(Write::Patch,0x169fc + uint32_t(index_)*256);next();break;
case Stage::WriteSavedPatch:write(Write::Patch,savedPatch_);next();break;
case Stage::WriteSavedProgram:write(Write::Program,savedProgram_);next();break;
case Stage::ReadCompareProgram:program_=uint16_t(read(Read::Program,0));next();break;
case Stage::SelectStandardBank:stage_=uint8_t(program_>>8)>4?Stage::CompareFxProgram:Stage::ResolveStandardPatch;break;
case Stage::ReadStandardPatch:target_=read(Read::Patch,0);next();break;
case Stage::BindStandardLength:remaining_=179;next();break;
case Stage::ReadStandardSource:sourceByte_=uint8_t(read(Read::Byte,source_++));next();break;
case Stage::ReadStandardTarget:targetByte_=uint8_t(read(Read::Byte,target_++));equal_=sourceByte_==targetByte_;next();break;
case Stage::SelectStandardMismatch:stage_=equal_?Stage::TickStandardLength:Stage::SelectStandardMatch;break;
case Stage::TickStandardLength:--remaining_;next();break;
case Stage::RepeatStandard:stage_=remaining_?Stage::ReadStandardSource:Stage::SelectStandardMatch;break;
case Stage::SelectStandardMatch:stage_=equal_&&remaining_==0?Stage::ShowChangedMarker:Stage::JoinStandardMismatch;break;
case Stage::SelectFxProgram:stage_=uint8_t(program_)==49?Stage::ResolveFxPatch:Stage::ResolveSplitPatch;break;
case Stage::ReadSplitPatch:target_=read(Read::Patch,0);next();break;
case Stage::BindPrefixLength:remaining_=84;next();break;
case Stage::ReadPrefixSource:sourceByte_=uint8_t(read(Read::Byte,source_++));next();break;
case Stage::ReadPrefixTarget:targetByte_=uint8_t(read(Read::Byte,target_++));equal_=sourceByte_==targetByte_;next();break;
case Stage::SelectPrefixMismatch:stage_=equal_?Stage::TickPrefixLength:Stage::SelectPrefixMatch;break;
case Stage::TickPrefixLength:--remaining_;next();break;
case Stage::RepeatPrefix:stage_=remaining_?Stage::ReadPrefixSource:Stage::SelectPrefixMatch;break;
case Stage::SelectPrefixMatch:stage_=equal_?Stage::SkipSourcePrefix:Stage::BindChangedMarker;break;
case Stage::SkipSourcePrefix:source_+=41;next();break;
case Stage::SkipTargetPrefix:target_+=41;next();break;
case Stage::ReadHeaderSource:sourceByte_=uint8_t(read(Read::Byte,source_));next();break;
case Stage::ReadHeaderTarget:targetByte_=uint8_t(read(Read::Byte,target_));equal_=sourceByte_==targetByte_;next();break;
case Stage::SelectHeaderMismatch:stage_=equal_?Stage::SkipSourceHeader:Stage::BindChangedMarker;break;
case Stage::SkipSourceHeader:source_+=2;next();break;
case Stage::SkipTargetHeader:target_+=2;next();break;
case Stage::BindSuffixLength:remaining_=82;next();break;
case Stage::ReadSuffixSource:sourceByte_=uint8_t(read(Read::Byte,source_++));next();break;
case Stage::ReadSuffixTarget:targetByte_=uint8_t(read(Read::Byte,target_++));equal_=sourceByte_==targetByte_;next();break;
case Stage::SelectSuffixMismatch:stage_=equal_?Stage::TickSuffixLength:Stage::SelectSuffixMatch;break;
case Stage::TickSuffixLength:--remaining_;next();break;
case Stage::RepeatSuffix:stage_=remaining_?Stage::ReadSuffixSource:Stage::SelectSuffixMatch;break;
case Stage::SelectSuffixMatch:stage_=equal_?Stage::JoinSplitMatch:Stage::BindChangedMarker;break;
case Stage::ReadFxPatch:target_=read(Read::Patch,0);next();break;
case Stage::SkipSourceFx:source_+=84;next();break;
case Stage::SkipTargetFx:target_+=84;next();break;
case Stage::BindFxLength:remaining_=41;next();break;
case Stage::ReadFxSource:sourceByte_=uint8_t(read(Read::Byte,source_++));next();break;
case Stage::ReadFxTarget:targetByte_=uint8_t(read(Read::Byte,target_++));equal_=sourceByte_==targetByte_;next();break;
case Stage::SelectFxMismatch:stage_=equal_?Stage::TickFxLength:Stage::SelectFxMatch;break;
case Stage::TickFxLength:--remaining_;next();break;
case Stage::RepeatFx:stage_=remaining_?Stage::ReadFxSource:Stage::SelectFxMatch;break;
case Stage::SelectFxMatch:stage_=equal_?Stage::SkipTargetTail:Stage::BindChangedMarker;break;
case Stage::SkipTargetTail:++target_;next();break;
case Stage::SkipSourceTail:++source_;next();break;
case Stage::ReadTailSource:sourceByte_=uint8_t(read(Read::Byte,source_));next();break;
case Stage::ReadTailTarget:targetByte_=uint8_t(read(Read::Byte,target_));equal_=sourceByte_==targetByte_;next();break;
case Stage::SelectTailMatch:stage_=equal_?Stage::ShowChangedMarker:Stage::BindChangedMarker;break;
case Stage::ReadCompareMarker:value_=uint8_t(read(Read::Marker,0));next();break;
case Stage::JoinSpecial:stage_=Stage::BindNormalPeriod;break;
case Stage::JoinMeter:stage_=Stage::BindNormalPeriod;break;
case Stage::JoinFastPeriod:stage_=Stage::BindStatusCallback;break;
case Stage::JoinSingleMarker:stage_=Stage::ReadCompareProgram;break;
case Stage::JoinStandardMismatch:stage_=Stage::BindChangedMarker;break;
case Stage::JoinSplitMatch:stage_=Stage::ShowChangedMarker;break;
case Stage::ShowSpecialCharacter:await(Stage::JoinSpecial);break;
case Stage::ReadStatusGate:await(Stage::SelectStatusGate);break;
case Stage::ShowMeter:await(Stage::JoinMeter);break;
case Stage::ReadMeterInput:await(Stage::BindMeterRange);break;
case Stage::ShowMeterValue:await(Stage::BindFastPeriod);break;
case Stage::ShowPartStatus:await(Stage::BindNormalPeriod);break;
case Stage::ScheduleStatus:await(Stage::ReturnStatus);break;
case Stage::ReturnStatus:stage_=Stage::Complete;break;
case Stage::ShowSinglePosition:await(Stage::BindSingleMarker);break;
case Stage::ShowScanPosition:await(Stage::BindInactiveMarker);break;
case Stage::ShowInactiveMarker:await(Stage::TickScanAttempts);break;
case Stage::ShowInactivePartMarker:await(Stage::ReturnInactive);break;
case Stage::ReturnInactive:stage_=Stage::Complete;break;
case Stage::CompareScanPatch:await(Stage::RestoreSavedPatch);break;
case Stage::ReturnActive:stage_=Stage::Complete;break;
case Stage::ResolveStandardPatch:await(Stage::ReadStandardPatch);break;
case Stage::ResolveSplitPatch:await(Stage::ReadSplitPatch);break;
case Stage::ResolveFxPatch:await(Stage::ReadFxPatch);break;
case Stage::ShowChangedMarker:await(Stage::ReadCompareMarker);break;
case Stage::ShowCompareMarker:await(Stage::ReturnCompare);break;
case Stage::ReturnCompare:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;void finish(Exit e){exit_=e;stage_=Stage::Complete;}Exit exit_=Exit::None;uint32_t source_=0,target_=0,savedPatch_=0;uint16_t program_=0,savedProgram_=0,meter_=0,raw_=0,remaining_=0;uint8_t busy_=0,value_=0,page_=0,index_=0,mask_=0,sourceByte_=0,targetByte_=0;bool gate_=false,equal_=false;
};
}
