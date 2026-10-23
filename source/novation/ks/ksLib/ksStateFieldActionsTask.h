#pragma once
#include <cstdint>
namespace ks {
// StateFieldActions semantic policy: logical data, decisions and deferred effects; compatibility frame stays in Calls.
class StateFieldActionsTask final {
public:
 enum class Kind { Global, Selector, Indexed, Clear, DirectStep, IndexedStep, Bit, Pair, Return, Current, Cycle, Control };enum class Stage { SaveGlobalEvent, BindGlobalContext, WriteGlobalContext, ResolveGlobalField, JoinGlobalField, SaveSelectorDescriptor, SaveSelectorIndex, SaveSelectorEvent, ReadSelectorValue, ReadSelectorPatch, WriteSelectorValue, RestoreSelectorEvent, RestoreSelectorIndex, RestoreSelectorDescriptor, SaveIndexedEvent, ResolveIndexedField, ReadValueType, ResolveValueBounds, TestValueMode, SelectValueMode, BindValueDelta, ReadCurrentValue, TestValueDelta, SelectValueDelta, CompareZeroTypeOne, SelectZeroTypeOne, CompareZeroTypeFourteen, SelectZeroTypeFourteen, DisplayZeroValue, RestoreZeroEvent, ReturnZeroValue, AddValueDelta, TestDeltaSign, SelectDeltaSign, CompareValueUpper, SelectValueUpper, ClampValueUpper, JoinValueUpper, CompareValueLower, SelectValueLower, ClampValueLower, WriteEditedField, ApplyEditedField, RestoreDisplayIndex, BindDisplayContext, ReadDisplayEnabled, SelectDisplayEnabled, ReadDisplayOverride, SelectDisplayOverride, ReadDisplayPatch, LocateDisplayOffset, SaveDisplayEvent, SaveDisplayOffset, BuildDisplayLabel, RestoreDisplayOffset, WriteDisplayField, ShowDisplayValue, BindDisplayLatch, WriteDisplayLatch, RestoreDisplayEvent, ReadDisplayGate, SelectDisplayGate, BindDisplayGuard, ReadDisplayGuard, SelectDisplayGuard, NormalizeDisplayDelay, ScaleDisplayDelayOne, ScaleDisplayDelayTwo, ScaleDisplayDelayThree, BiasDisplayDelay, BindDisplayCallback, ScheduleDisplay, ReturnDisplay, ClearDisplayLatch, WriteClearedDisplayLatch, RefreshDisplay, DirectAddress, StepDirect, ReadDirectPatch, LocateDirectOffset, CompareDirectField, SelectDirectField, ApplyDirectSideEffect, ReturnDirect, IndexedAddress, StepIndexed, ReturnIndexed, ResolveBitAddress, ClearBitSelector, ReadBitSelector, ReadBitPatch, LocateBitSelector, ClearBitIndex, ReadBitIndex, LocateBitMapping, ReadBitMapping, ReadBitControl, BindBitRequest, ReadBitValue, TestBitRequest, SelectBitRequest, ToggleBit, SaveBitValue, SaveBitRequest, ApplyBit, RestoreBitRequest, RestoreBitValue, TestBitShift, SelectBitShift, ShiftBitValue, TickBitShift, RepeatBitShift, MaskBitValue, DisplayBit, ResolvePairAddress, ClearPairSelector, ReadPairSelector, ReadPairPatch, LocatePairSelector, ClearPairIndex, ReadPairIndex, LocatePairMapping, ReadPairMapping, BindPairRequest, ReadPairValue, SavePairRequest, TestPairShift, SelectPairShift, RotatePairRight, TickPairShift, RepeatPairRight, BindPairUpper, MaskPairUpper, MaskPairValue, SavePairValue, CyclePair, RestorePairValue, RestorePairRequest, MergePairValue, TestPairRestore, SelectPairRestore, RotatePairLeft, TickPairRestore, RepeatPairLeft, TestPairRequest, SelectPairRequest, ReturnPair, ReturnAddress, RetainCycleRequest, ReadCycleValue, CycleCurrentValue, TestCycleRequest, SelectCycleRequest, ApplyCycleValue, ReturnCycleValue, TestCycleMode, SelectCycleMode, ReadCycleLimit, IncrementCycleValue, CompareCycleLimit, SelectCycleLimit, ClearCycleValue, ReturnCycle, ReadControlFlag, DisplayControlFlag, AwaitChild, Complete };
 enum class Exit { None, Refresh, Display, Apply };Exit exit()const{return exit_;}enum class Read { Byte, Patch, Context };enum class Write { Byte, Context, EditedField, DisplayField, Latch };uint32_t selector()const{return selector_;}uint32_t patch()const{return patch_;}uint32_t type()const{return type_;}uint32_t value()const{return value_;}uint32_t enabled()const{return enabled_;}uint32_t base()const{return base_;}uint32_t guard()const{return guard_;}uint32_t index()const{return index_;}uint32_t bit()const{return bit_;}uint32_t control()const{return control_;}uint32_t limit()const{return limit_;}
 explicit StateFieldActionsTask(Kind kind,uint32_t descriptor=0,uint32_t pointer=0,uint16_t event=0,uint16_t mode=0,uint16_t limits=0,uint8_t request=0){descriptor_=descriptor;pointer_=pointer;value_=uint8_t(event>>8);request_=request;absolute_=bool(mode>>8);field_=uint8_t(mode);bounds_=limits;switch(kind){case Kind::Global:stage_=Stage::SaveGlobalEvent;break;case Kind::Selector:stage_=Stage::SaveSelectorDescriptor;break;case Kind::Indexed:stage_=Stage::SaveIndexedEvent;break;case Kind::Clear:stage_=Stage::ClearDisplayLatch;break;case Kind::DirectStep:stage_=Stage::DirectAddress;break;case Kind::IndexedStep:stage_=Stage::IndexedAddress;break;case Kind::Bit:stage_=Stage::ResolveBitAddress;break;case Kind::Pair:stage_=Stage::ResolvePairAddress;break;case Kind::Return:stage_=Stage::ReturnAddress;break;case Kind::Current:stage_=Stage::RetainCycleRequest;break;case Kind::Cycle:stage_=Stage::TestCycleMode;break;case Kind::Control:stage_=Stage::ReadControlFlag;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint32_t pointer,uint8_t value,uint16_t bounds,bool gate){if(!awaitingChild())return false;if(resume_==Stage::JoinGlobalField||resume_==Stage::ReadValueType||resume_==Stage::StepDirect||resume_==Stage::StepIndexed||resume_==Stage::ClearBitSelector||resume_==Stage::ClearPairSelector)pointer_=pointer;if(resume_==Stage::TestValueMode){pointer_=pointer;value_=value;bounds_=bounds;}if(resume_==Stage::TestCycleRequest||resume_==Stage::RestorePairValue)value_=value;if(resume_==Stage::SelectDisplayGate)gate_=gate;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::SaveGlobalEvent:case Stage::SaveSelectorIndex:case Stage::SaveSelectorEvent:case Stage::RestoreSelectorEvent:case Stage::RestoreSelectorIndex:case Stage::SaveIndexedEvent:case Stage::RestoreZeroEvent:case Stage::RestoreDisplayIndex:case Stage::SaveDisplayEvent:case Stage::SaveDisplayOffset:case Stage::RestoreDisplayOffset:case Stage::RestoreDisplayEvent:case Stage::SaveBitValue:case Stage::SaveBitRequest:case Stage::RestoreBitRequest:case Stage::RestoreBitValue:case Stage::SavePairRequest:case Stage::SavePairValue:case Stage::RestorePairValue:case Stage::RestorePairRequest:return 11; case Stage::BindGlobalContext:case Stage::BindDisplayContext:case Stage::BindDisplayGuard:case Stage::BindDisplayCallback:return 9; case Stage::WriteGlobalContext:case Stage::ReadSelectorValue:case Stage::ReadSelectorPatch:case Stage::WriteSelectorValue:case Stage::ReadValueType:case Stage::ReadDisplayEnabled:case Stage::ReadDisplayOverride:case Stage::ReadDisplayPatch:case Stage::ReadDisplayGuard:case Stage::ReadDirectPatch:case Stage::ReadBitSelector:case Stage::ReadBitPatch:case Stage::ReadBitMapping:case Stage::ReadBitControl:case Stage::ReadPairSelector:case Stage::ReadPairPatch:case Stage::ReadPairMapping:case Stage::ReadCycleLimit:case Stage::ReadControlFlag:return 12+store; case Stage::ResolveGlobalField:case Stage::SaveSelectorDescriptor:case Stage::RestoreSelectorDescriptor:case Stage::ResolveIndexedField:case Stage::ResolveValueBounds:case Stage::DisplayZeroValue:case Stage::ApplyEditedField:case Stage::ShowDisplayValue:case Stage::ReadDisplayGate:case Stage::ScheduleDisplay:case Stage::DirectAddress:case Stage::StepDirect:case Stage::ApplyDirectSideEffect:case Stage::IndexedAddress:case Stage::StepIndexed:case Stage::ResolveBitAddress:case Stage::ApplyBit:case Stage::ResolvePairAddress:case Stage::CyclePair:case Stage::ApplyCycleValue:return 20; case Stage::ReadCurrentValue:case Stage::ReadBitIndex:case Stage::ReadBitValue:case Stage::ReadPairIndex:case Stage::ReadPairValue:case Stage::ReadCycleValue:return 3+store; case Stage::ReturnZeroValue:case Stage::ReturnDisplay:case Stage::ReturnDirect:case Stage::ReturnIndexed:case Stage::ReturnPair:case Stage::ReturnAddress:case Stage::ReturnCycleValue:case Stage::ReturnCycle:return 17; case Stage::WriteEditedField:case Stage::WriteDisplayField:case Stage::WriteDisplayLatch:case Stage::WriteClearedDisplayLatch:return 9+store; case Stage::BuildDisplayLabel:case Stage::CycleCurrentValue:return 15; case Stage::BiasDisplayDelay:return 6; case Stage::RefreshDisplay:case Stage::DisplayBit:case Stage::SelectPairRequest:case Stage::DisplayControlFlag:return 8; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::WriteGlobalContext:write(Write::Context,0,0x168fc);next();break;
case Stage::ReadSelectorValue:selector_=uint8_t(read(Read::Byte,descriptor_+1));next();break;
case Stage::ReadSelectorPatch:patch_=read(Read::Patch,0);next();break;
case Stage::WriteSelectorValue:write(Write::Byte,patch_+222,selector_);next();break;
case Stage::ReadValueType:type_=uint8_t(read(Read::Byte,descriptor_+6));next();break;
case Stage::SelectValueMode:stage_=absolute_?Stage::BindValueDelta:Stage::WriteEditedField;break;
case Stage::BindValueDelta:delta_=value_;next();break;
case Stage::ReadCurrentValue:value_=uint8_t(read(Read::Byte,pointer_));next();break;
case Stage::SelectValueDelta:stage_=delta_?Stage::AddValueDelta:Stage::CompareZeroTypeOne;break;
case Stage::SelectZeroTypeOne:stage_=field_==1?Stage::DisplayZeroValue:Stage::CompareZeroTypeFourteen;break;
case Stage::SelectZeroTypeFourteen:stage_=field_!=14?Stage::RestoreDisplayIndex:Stage::DisplayZeroValue;break;
case Stage::AddValueDelta:value_=uint8_t(value_+delta_);next();break;
case Stage::SelectDeltaSign:stage_=delta_&128?Stage::CompareValueLower:Stage::CompareValueUpper;break;
case Stage::SelectValueUpper:stage_=value_<=uint8_t(bounds_>>8)?Stage::WriteEditedField:Stage::ClampValueUpper;break;
case Stage::ClampValueUpper:value_=uint8_t(bounds_>>8);next();break;
case Stage::SelectValueLower:stage_=int8_t(value_)>=int8_t(uint8_t(bounds_))?Stage::WriteEditedField:Stage::ClampValueLower;break;
case Stage::ClampValueLower:value_=uint8_t(bounds_);next();break;
case Stage::WriteEditedField:write(Write::EditedField,0,field_);next();break;
case Stage::ReadDisplayEnabled:enabled_=uint8_t(read(Read::Byte,0x169f6));next();break;
case Stage::SelectDisplayEnabled:stage_=enabled_?Stage::ReadDisplayOverride:Stage::ReturnDisplay;break;
case Stage::ReadDisplayOverride:base_=read(Read::Context,0);next();break;
case Stage::SelectDisplayOverride:stage_=base_?Stage::LocateDisplayOffset:Stage::ReadDisplayPatch;break;
case Stage::ReadDisplayPatch:base_=read(Read::Patch,0);next();break;
case Stage::LocateDisplayOffset:offset_=pointer_-base_;next();break;
case Stage::WriteDisplayField:write(Write::DisplayField,0,uint8_t(offset_));next();break;
case Stage::WriteDisplayLatch:write(Write::Latch,0,1);next();break;
case Stage::SelectDisplayGate:stage_=gate_?Stage::NormalizeDisplayDelay:Stage::BindDisplayGuard;break;
case Stage::ReadDisplayGuard:guard_=uint8_t(read(Read::Byte,0x169f8));next();break;
case Stage::SelectDisplayGuard:stage_=guard_?Stage::ReturnDisplay:Stage::NormalizeDisplayDelay;break;
case Stage::WriteClearedDisplayLatch:write(Write::Latch,0,0);next();break;
case Stage::ReadDirectPatch:patch_=read(Read::Patch,0);next();break;
case Stage::LocateDirectOffset:offset_=pointer_-patch_;next();break;
case Stage::SelectDirectField:stage_=uint8_t(offset_)!=225?Stage::ReturnDirect:Stage::ApplyDirectSideEffect;break;
case Stage::ReadBitSelector:selector_=uint8_t(read(Read::Byte,descriptor_+2));next();break;
case Stage::ReadBitPatch:patch_=read(Read::Patch,0);next();break;
case Stage::LocateBitSelector:mapping_=patch_+selector_;next();break;
case Stage::ReadBitIndex:index_=uint8_t(read(Read::Byte,mapping_));next();break;
case Stage::ReadBitMapping:bit_=uint8_t(read(Read::Byte,descriptor_+index_+4));next();break;
case Stage::ReadBitControl:control_=uint8_t(read(Read::Byte,descriptor_+7));next();break;
case Stage::BindBitRequest:request_=value_;next();break;
case Stage::ReadBitValue:value_=uint8_t(read(Read::Byte,pointer_));next();break;
case Stage::SelectBitRequest:stage_=request_?Stage::ToggleBit:Stage::TestBitShift;break;
case Stage::ToggleBit:value_^=uint8_t(1u<<(bit_&7));next();break;
case Stage::SelectBitShift:stage_=bit_?Stage::ShiftBitValue:Stage::MaskBitValue;break;
case Stage::ShiftBitValue:value_>>=1;next();break;
case Stage::TickBitShift:--bit_;next();break;
case Stage::RepeatBitShift:stage_=bit_?Stage::ShiftBitValue:Stage::MaskBitValue;break;
case Stage::MaskBitValue:value_&=1;next();break;
case Stage::ReadPairSelector:selector_=uint8_t(read(Read::Byte,descriptor_+2));next();break;
case Stage::ReadPairPatch:patch_=read(Read::Patch,0);next();break;
case Stage::LocatePairSelector:mapping_=patch_+selector_;next();break;
case Stage::ReadPairIndex:index_=uint8_t(read(Read::Byte,mapping_));next();break;
case Stage::ReadPairMapping:bit_=uint8_t(read(Read::Byte,descriptor_+index_+4));next();break;
case Stage::BindPairRequest:request_=value_;next();break;
case Stage::ReadPairValue:value_=uint8_t(read(Read::Byte,pointer_));next();break;
case Stage::SavePairRequest:savedBit_=bit_;next();break;
case Stage::SelectPairShift:stage_=bit_?Stage::RotatePairRight:Stage::BindPairUpper;break;
case Stage::RotatePairRight:value_=uint8_t((value_>>1)|(value_<<7));next();break;
case Stage::TickPairShift:--bit_;next();break;
case Stage::RepeatPairRight:stage_=bit_?Stage::RotatePairRight:Stage::BindPairUpper;break;
case Stage::MaskPairUpper:upper_=value_&252;next();break;
case Stage::MaskPairValue:value_&=3;next();break;
case Stage::RestorePairRequest:bit_=savedBit_;next();break;
case Stage::MergePairValue:value_|=upper_;next();break;
case Stage::SelectPairRestore:stage_=bit_?Stage::RotatePairLeft:Stage::TestPairRequest;break;
case Stage::RotatePairLeft:value_=uint8_t((value_<<1)|(value_>>7));next();break;
case Stage::TickPairRestore:--bit_;next();break;
case Stage::RepeatPairLeft:stage_=bit_?Stage::RotatePairLeft:Stage::TestPairRequest;break;
case Stage::SelectPairRequest:if(request_)finish(Exit::Apply);else next();break;
case Stage::RetainCycleRequest:request_=value_;next();break;
case Stage::ReadCycleValue:value_=uint8_t(read(Read::Byte,pointer_));next();break;
case Stage::SelectCycleRequest:stage_=request_?Stage::ApplyCycleValue:Stage::ReturnCycleValue;break;
case Stage::SelectCycleMode:stage_=request_?Stage::ReadCycleLimit:Stage::ReadControlFlag;break;
case Stage::ReadCycleLimit:limit_=uint8_t(read(Read::Byte,descriptor_+1));next();break;
case Stage::IncrementCycleValue:++value_;next();break;
case Stage::SelectCycleLimit:stage_=value_<=limit_?Stage::ReturnCycle:Stage::ClearCycleValue;break;
case Stage::ClearCycleValue:value_=0;next();break;
case Stage::ReadControlFlag:control_=uint8_t(read(Read::Byte,descriptor_+7));next();break;
case Stage::JoinGlobalField:stage_=Stage::ReadValueType;break;
case Stage::JoinValueUpper:stage_=Stage::WriteEditedField;break;
case Stage::RefreshDisplay:finish(Exit::Refresh);break;
case Stage::DisplayBit:finish(Exit::Display);break;
case Stage::DisplayControlFlag:finish(Exit::Display);break;
case Stage::ResolveGlobalField:await(Stage::JoinGlobalField);break;
case Stage::ResolveIndexedField:await(Stage::ReadValueType);break;
case Stage::ResolveValueBounds:await(Stage::TestValueMode);break;
case Stage::DisplayZeroValue:await(Stage::RestoreZeroEvent);break;
case Stage::ReturnZeroValue:stage_=Stage::Complete;break;
case Stage::ApplyEditedField:await(Stage::RestoreDisplayIndex);break;
case Stage::BuildDisplayLabel:await(Stage::RestoreDisplayOffset);break;
case Stage::ShowDisplayValue:await(Stage::BindDisplayLatch);break;
case Stage::ReadDisplayGate:await(Stage::SelectDisplayGate);break;
case Stage::ScheduleDisplay:await(Stage::ReturnDisplay);break;
case Stage::ReturnDisplay:stage_=Stage::Complete;break;
case Stage::DirectAddress:await(Stage::StepDirect);break;
case Stage::StepDirect:await(Stage::ReadDirectPatch);break;
case Stage::ApplyDirectSideEffect:await(Stage::ReturnDirect);break;
case Stage::ReturnDirect:stage_=Stage::Complete;break;
case Stage::IndexedAddress:await(Stage::StepIndexed);break;
case Stage::StepIndexed:await(Stage::ReturnIndexed);break;
case Stage::ReturnIndexed:stage_=Stage::Complete;break;
case Stage::ResolveBitAddress:await(Stage::ClearBitSelector);break;
case Stage::ApplyBit:await(Stage::RestoreBitRequest);break;
case Stage::ResolvePairAddress:await(Stage::ClearPairSelector);break;
case Stage::CyclePair:await(Stage::RestorePairValue);break;
case Stage::ReturnPair:stage_=Stage::Complete;break;
case Stage::ReturnAddress:stage_=Stage::Complete;break;
case Stage::CycleCurrentValue:await(Stage::TestCycleRequest);break;
case Stage::ApplyCycleValue:await(Stage::ReturnCycleValue);break;
case Stage::ReturnCycleValue:stage_=Stage::Complete;break;
case Stage::ReturnCycle:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;void finish(Exit e){exit_=e;stage_=Stage::Complete;}Exit exit_=Exit::None;uint32_t descriptor_=0,pointer_=0,patch_=0,base_=0,offset_=0,mapping_=0;uint16_t bounds_=0;uint8_t field_=0,type_=0,value_=0,delta_=0,selector_=0,index_=0,bit_=0,savedBit_=0,upper_=0,control_=0,request_=0,limit_=0,enabled_=0,guard_=0;bool absolute_=false,gate_=false;
};
}
