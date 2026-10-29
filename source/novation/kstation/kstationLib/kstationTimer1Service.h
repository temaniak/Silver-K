#pragma once
#include <cstdint>
namespace kstation {
// KS-style typed peripheral continuation for the complete K Timer1 graph.
// All state is encoder/clock-divider/panel data; CPU projection is separate.
class Timer1Service final {
public:
 enum class Stage { RetainTimer,ReadTimerStatus,ClearTimerValue,PublishTimerStatus,DispatchScan,ReadDivider,AdvanceDivider,PublishDivider,ChooseClock,DispatchClock,ReadTick,IncrementTick,PublishTick,MaskTick,ChoosePanel,DispatchPanel,RestoreTimer,FinishTimer,SaveScanIndex,SaveScanContacts,SaveScanRecord,ReadScanTick,IncrementScanTick,PublishScanTick,BindHistory,ReadPins,SelectMux,PublishMux,ReadHistory,ComparePins,ChooseChanges,RestoreScanRecord,RestoreScanContacts,RestoreScanIndex,ReturnScan,BindChangedRecord,DispatchChanges,ResumeScan,PublishHistory,RestoreHistoryCursor,SaveTransitionIndex,ClearTransition,ShiftCurrentFirst,AppendCurrentFirst,ShiftCurrentSecond,AppendCurrentSecond,ShiftPreviousFirst,AppendPreviousFirst,ShiftPreviousSecond,AppendPreviousSecond,ScaleTransition,BindTransition,DispatchTransition,NextTransitionRecord,RestoreTransitionIndex,ReturnChanges,Transition,ReadPressTick,StorePressTick,SetPressed,ReturnPress,ReadReleaseTick,StoreReleaseTick,ClearPressed,ReturnRelease,ReadFixedOnKey,BindFixedOnSpeed,AppendFixedOn,ReadFixedOffKey,BindFixedOffSpeed,AppendFixedOff,ReadRepeatFlags,ChooseRepeat,ReadRepeatTick,ReadRepeatStart,ElapsedRepeat,BiasRepeat,ChooseEarlyRepeat,IncrementRepeat,CalculateRepeat,MarkRepeat,ReadRepeatKey,AppendRepeat,ReturnRepeat,ReadReleaseFlags,ChooseRelease,ReadVelocityTick,ReadVelocityStart,ElapsedVelocity,CalculateRelease,ReadReleaseKey,AppendRelease,ReturnReleaseSpeed,SaveSpeedValue,SaveSpeedIndex,RetainDelay,BindSlowPeriod,BindSlowBias,ReadSpeedFlags,ChooseSpeedMode,BindFastPeriod,BindFastBias,SubtractDelayBias,ChooseDelayFloor,ClearDelayFloor,ClearNumerator,SquareDelay,BindDenominatorBias,NormalizeDenominatorBias,AddDenominatorBias,HalfNumerator,TestDenominatorHigh,ChooseDenominatorShift,ShiftDenominator,ShiftNumerator,RepeatDenominator,DivideSpeed,IncrementSpeed,ChooseSpeedLimit,ClampSpeed,RestoreSpeedIndex,RestoreSpeedValue,ReturnSpeed,BiasEvent,SaveAppendIndex,SaveAppendValue,ReadProducer,NormalizeProducer,BindQueue,LocateEvent,PublishEvent,IncrementProducer,MaskProducer,PublishProducer,RestoreAppendValue,RestoreAppendIndex,ReturnAppend,ReadPanelCount,ChoosePanelActive,DecayPanel,PublishPanelCount,ChoosePanelExpiry,RetainPanelPort,ReadPanelPort,MaskPanelPort,ReadPanelMirror,MaskPanelMirror,MergePanel,PublishPanelPort,RestorePanelPort,RetainPanelCommand,SetPanelCommand,DispatchPanelCommand,RestorePanelCommand,ReturnPanel,RetainCommand,DispatchReadyPoll,RestoreCommand,DispatchCommandWrite,ReturnCommand,PollFirstRead,PollTest,PollChoose,PollFinalRead,PollReturn,RetainRead,ReadDirectionValue,ReadDirection,ReadSelectValue,ReadSelect,ReadEnableValue,ReadEnable,ReadWaitFirst,ReadWaitSecond,ReadWaitThird,ReadLcdData,ReadFinishValue,ReadFinish,ReadRestoreDirectionValue,ReadRestoreDirection,RestoreRead,ReturnRead,RetainWrite,WriteSelectValue,WriteSelect,WriteEnableValue,WriteEnable,WriteLcdData,WriteWaitFirst,WriteWaitSecond,WriteWaitThird,WriteFinishValue,WriteFinish,RestoreWrite,ReturnWrite,Complete };
 using S=Stage;
 enum class Read {TimerStatus,Divider,Tick,ScanTick,Pins,History,EventTick,EventStart,EventKey,RecordFlagsKeep,RecordFlagsTest,SpeedFlags,Producer,PanelCount,PanelPort,PanelMirror,LcdData};
 enum class Write {TimerStatus,Divider,Tick,ScanTick,Mux,History,RecordTick,RecordFlags,Event,Producer,PanelCount,PanelPort,LcdLow,LcdCommand};
 using R=Read;using W=Write;
 struct Credits {unsigned stack=0,read=0,write=0;};
 Stage stage()const{return stage_;} bool complete()const{return stage_==S::Complete;} bool awaitingClock()const{return awaitClock_;} bool resumeClock(){if(!awaitClock_)return false;awaitClock_=false;return true;}
 unsigned page()const{return page_;} unsigned quad()const{return quad_;} unsigned panel()const{return panel_;} unsigned transition()const{return transition_;}
 uint32_t history()const{return 0xa856u+page_;} uint32_t pageRecord()const{return 0xa86au+32*page_;} uint32_t record()const{return pageRecord()+4*quad_;} uint32_t queueRef()const{return 0xc6aeu+producer_;}
 uint8_t muxValue()const{return uint8_t(1u<<((page_+1)&7));}
 bool transitionReturns()const{return transition_==0||transition_==2||transition_==5||transition_==6||transition_==8||transition_==9||transition_==10||transition_==11||transition_==14||transition_==15;}
 unsigned nextClocks(const Credits& c)const {if(awaitClock_)return 0;switch(stage_){
 case S::RetainTimer:return c.stack;
 case S::ReadTimerStatus:return 3 + c.read;
 case S::ClearTimerValue:return 3;
 case S::PublishTimerStatus:return 3 + c.write;
 case S::DispatchScan:return c.stack;
 case S::ReadDivider:return 9 + c.read;
 case S::AdvanceDivider:return 3;
 case S::PublishDivider:return 9 + c.write;
 case S::ChooseClock:return 3;
 case S::DispatchClock:return c.stack;
 case S::ReadTick:return 12 + c.read;
 case S::IncrementTick:return 3;
 case S::PublishTick:return 12 + c.write;
 case S::MaskTick:return 3;
 case S::ChoosePanel:return 3;
 case S::DispatchPanel:return c.stack;
 case S::RestoreTimer:return c.stack;
 case S::FinishTimer:return c.stack;
 case S::SaveScanIndex:return c.stack;
 case S::SaveScanContacts:return c.stack;
 case S::SaveScanRecord:return c.stack;
 case S::ReadScanTick:return 9 + c.read;
 case S::IncrementScanTick:return 3;
 case S::PublishScanTick:return 9 + c.write;
 case S::BindHistory:return 9;
 case S::ReadPins:return 3 + c.read;
 case S::SelectMux:return 3;
 case S::PublishMux:return 3 + c.write;
 case S::ReadHistory:return 5 + c.read;
 case S::ComparePins:return 3;
 case S::ChooseChanges:return 3;
 case S::RestoreScanRecord:return c.stack;
 case S::RestoreScanContacts:return c.stack;
 case S::RestoreScanIndex:return c.stack;
 case S::ReturnScan:return c.stack;
 case S::BindChangedRecord:return 9;
 case S::DispatchChanges:return c.stack;
 case S::ResumeScan:return 3;
 case S::PublishHistory:return 5 + c.write;
 case S::RestoreHistoryCursor:return 3;
 case S::SaveTransitionIndex:return c.stack;
 case S::ClearTransition:return 3;
 case S::ShiftCurrentFirst:return 3;
 case S::AppendCurrentFirst:return 3;
 case S::ShiftCurrentSecond:return 3;
 case S::AppendCurrentSecond:return 3;
 case S::ShiftPreviousFirst:return 3;
 case S::AppendPreviousFirst:return 3;
 case S::ShiftPreviousSecond:return 3;
 case S::AppendPreviousSecond:return 3;
 case S::ScaleTransition:return 3;
 case S::BindTransition:return 9;
 case S::DispatchTransition:return c.stack;
 case S::NextTransitionRecord:return 3;
 case S::RestoreTransitionIndex:return c.stack;
 case S::ReturnChanges:return c.stack;
 case S::Transition:return transitionReturns()?c.stack:3;
 case S::ReadPressTick:return 9 + c.read;
 case S::StorePressTick:return 6 + c.write;
 case S::SetPressed:return 6 + c.read + c.write;
 case S::ReturnPress:return c.stack;
 case S::ReadReleaseTick:return 9 + c.read;
 case S::StoreReleaseTick:return 6 + c.write;
 case S::ClearPressed:return 6 + c.read + c.write;
 case S::ReturnRelease:return c.stack;
 case S::ReadFixedOnKey:return 6 + c.read;
 case S::BindFixedOnSpeed:return 3;
 case S::AppendFixedOn:return 8;
 case S::ReadFixedOffKey:return 6 + c.read;
 case S::BindFixedOffSpeed:return 3;
 case S::AppendFixedOff:return 8;
 case S::ReadRepeatFlags:return 6 + c.read;
 case S::ChooseRepeat:return 3;
 case S::ReadRepeatTick:return 9 + c.read;
 case S::ReadRepeatStart:return 6 + c.read;
 case S::ElapsedRepeat:return 3;
 case S::BiasRepeat:return 6;
 case S::ChooseEarlyRepeat:return 3;
 case S::IncrementRepeat:return 3;
 case S::CalculateRepeat:return c.stack;
 case S::MarkRepeat:return 3;
 case S::ReadRepeatKey:return 6 + c.read;
 case S::AppendRepeat:return 8;
 case S::ReturnRepeat:return c.stack;
 case S::ReadReleaseFlags:return 6 + c.read;
 case S::ChooseRelease:return 3;
 case S::ReadVelocityTick:return 9 + c.read;
 case S::ReadVelocityStart:return 6 + c.read;
 case S::ElapsedVelocity:return 3;
 case S::CalculateRelease:return c.stack;
 case S::ReadReleaseKey:return 6 + c.read;
 case S::AppendRelease:return 8;
 case S::ReturnReleaseSpeed:return c.stack;
 case S::SaveSpeedValue:return c.stack;
 case S::SaveSpeedIndex:return c.stack;
 case S::RetainDelay:return 3;
 case S::BindSlowPeriod:return 6;
 case S::BindSlowBias:return 6;
 case S::ReadSpeedFlags:return 6 + c.read;
 case S::ChooseSpeedMode:return 3;
 case S::BindFastPeriod:return 6;
 case S::BindFastBias:return 6;
 case S::SubtractDelayBias:return 3;
 case S::ChooseDelayFloor:return 3;
 case S::ClearDelayFloor:return 3;
 case S::ClearNumerator:return 3;
 case S::SquareDelay:return 23;
 case S::BindDenominatorBias:return 3;
 case S::NormalizeDenominatorBias:return 3;
 case S::AddDenominatorBias:return 3;
 case S::HalfNumerator:return 3;
 case S::TestDenominatorHigh:return 3;
 case S::ChooseDenominatorShift:return 3;
 case S::ShiftDenominator:return 3;
 case S::ShiftNumerator:return 3;
 case S::RepeatDenominator:return 3;
 case S::DivideSpeed:return 23;
 case S::IncrementSpeed:return 3;
 case S::ChooseSpeedLimit:return 3;
 case S::ClampSpeed:return 3;
 case S::RestoreSpeedIndex:return c.stack;
 case S::RestoreSpeedValue:return c.stack;
 case S::ReturnSpeed:return c.stack;
 case S::BiasEvent:return 3;
 case S::SaveAppendIndex:return c.stack;
 case S::SaveAppendValue:return c.stack;
 case S::ReadProducer:return 9 + c.read;
 case S::NormalizeProducer:return 3;
 case S::BindQueue:return 9;
 case S::LocateEvent:return 3;
 case S::PublishEvent:return 3 + c.write;
 case S::IncrementProducer:return 3;
 case S::MaskProducer:return 6;
 case S::PublishProducer:return 9 + c.write;
 case S::RestoreAppendValue:return c.stack;
 case S::RestoreAppendIndex:return c.stack;
 case S::ReturnAppend:return c.stack;
 case S::ReadPanelCount:return 9 + c.read;
 case S::ChoosePanelActive:return 3;
 case S::DecayPanel:return 3;
 case S::PublishPanelCount:return 9 + c.write;
 case S::ChoosePanelExpiry:return 3;
 case S::RetainPanelPort:return c.stack;
 case S::ReadPanelPort:return 3 + c.read;
 case S::MaskPanelPort:return 3;
 case S::ReadPanelMirror:return 9 + c.read;
 case S::MaskPanelMirror:return 3;
 case S::MergePanel:return 3;
 case S::PublishPanelPort:return 3 + c.write;
 case S::RestorePanelPort:return c.stack;
 case S::RetainPanelCommand:return c.stack;
 case S::SetPanelCommand:return 3;
 case S::DispatchPanelCommand:return c.stack;
 case S::RestorePanelCommand:return c.stack;
 case S::ReturnPanel:return c.stack;
 case S::RetainCommand:return c.stack;
 case S::DispatchReadyPoll:return c.stack;
 case S::RestoreCommand:return c.stack;
 case S::DispatchCommandWrite:return c.stack;
 case S::ReturnCommand:return c.stack;
 case S::PollFirstRead:return c.stack;
 case S::PollTest:return 3;
 case S::PollChoose:return 3;
 case S::PollFinalRead:return c.stack;
 case S::PollReturn:return c.stack;
 case S::RetainRead:return c.stack;
 case S::ReadDirectionValue:return 3;
 case S::ReadDirection:return 3 + c.write;
 case S::ReadSelectValue:return 3;
 case S::ReadSelect:return 3 + c.write;
 case S::ReadEnableValue:return 3;
 case S::ReadEnable:return 3 + c.write;
 case S::ReadWaitFirst:return 3;
 case S::ReadWaitSecond:return 3;
 case S::ReadWaitThird:return 3;
 case S::ReadLcdData:return 3 + c.read;
 case S::ReadFinishValue:return 3;
 case S::ReadFinish:return 3 + c.write;
 case S::ReadRestoreDirectionValue:return 3;
 case S::ReadRestoreDirection:return 3 + c.write;
 case S::RestoreRead:return c.stack;
 case S::ReturnRead:return c.stack;
 case S::RetainWrite:return c.stack;
 case S::WriteSelectValue:return 3;
 case S::WriteSelect:return 3 + c.write;
 case S::WriteEnableValue:return 3;
 case S::WriteEnable:return 3 + c.write;
 case S::WriteLcdData:return 3 + c.write;
 case S::WriteWaitFirst:return 3;
 case S::WriteWaitSecond:return 3;
 case S::WriteWaitThird:return 3;
 case S::WriteFinishValue:return 3;
 case S::WriteFinish:return 3 + c.write;
 case S::RestoreWrite:return c.stack;
 case S::ReturnWrite:return c.stack;
 case S::Complete:return 0;}return 0;}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits&credits,Reader&&read,Writer&&write){const auto cost=nextClocks(credits);if(!cost||cost>available)return 0;switch(stage_){
 case S::RetainTimer:{next();break;}
 case S::ReadTimerStatus:{status_=uint8_t(read(R::TimerStatus,0xffff71));next();break;}
 case S::ClearTimerValue:{status_=0;next();break;}
 case S::PublishTimerStatus:{write(W::TimerStatus,0xffff71,status_);next();break;}
 case S::DispatchScan:{go(S::SaveScanIndex);break;}
 case S::ReadDivider:{divider_=uint8_t(read(R::Divider,0xfffd2a));next();break;}
 case S::AdvanceDivider:{dividerCarry_=divider_>=128;divider_=uint8_t(divider_+128);next();break;}
 case S::PublishDivider:{write(W::Divider,0xfffd2a,divider_);next();break;}
 case S::ChooseClock:{go(dividerCarry_?S::DispatchClock:S::RestoreTimer);break;}
 case S::DispatchClock:{awaitClock_=true;go(S::ReadTick);break;}
 case S::ReadTick:{tick_=read(R::Tick,0xfffd20);next();break;}
 case S::IncrementTick:{++tick_;next();break;}
 case S::PublishTick:{write(W::Tick,0xfffd20,tick_);next();break;}
 case S::MaskTick:{tickTail_=uint8_t(tick_)&7;next();break;}
 case S::ChoosePanel:{go(tickTail_?S::RestoreTimer:S::DispatchPanel);break;}
 case S::DispatchPanel:{panel_=0;go(S::ReadPanelCount);break;}
 case S::RestoreTimer:{next();break;}
 case S::FinishTimer:{go(S::Complete);break;}
 case S::SaveScanIndex:{next();break;}
 case S::SaveScanContacts:{next();break;}
 case S::SaveScanRecord:{next();break;}
 case S::ReadScanTick:{scanTick_=uint16_t(read(R::ScanTick,0xfffe86));next();break;}
 case S::IncrementScanTick:{++scanTick_;next();break;}
 case S::PublishScanTick:{write(W::ScanTick,0xfffe86,scanTick_);next();break;}
 case S::BindHistory:{page_=0;next();break;}
 case S::ReadPins:{sample_=uint8_t(read(R::Pins,0xffffd7));next();break;}
 case S::SelectMux:{mux_=uint8_t(1u<<((page_+1)&7));next();break;}
 case S::PublishMux:{write(W::Mux,0xffffd6,mux_);next();break;}
 case S::ReadHistory:{previous_=uint8_t(read(R::History,history()));next();break;}
 case S::ComparePins:{next();break;}
 case S::ChooseChanges:{if(sample_!=previous_)go(S::BindChangedRecord);else advancePage();break;}
 case S::RestoreScanRecord:{next();break;}
 case S::RestoreScanContacts:{next();break;}
 case S::RestoreScanIndex:{next();break;}
 case S::ReturnScan:{go(S::ReadDivider);break;}
 case S::BindChangedRecord:{quad_=0;next();break;}
 case S::DispatchChanges:{currentShift_=sample_;previousShift_=previous_;go(S::PublishHistory);break;}
 case S::ResumeScan:{advancePage();break;}
 case S::PublishHistory:{write(W::History,history(),sample_);next();break;}
 case S::RestoreHistoryCursor:{next();break;}
 case S::SaveTransitionIndex:{next();break;}
 case S::ClearTransition:{transition_=0;next();break;}
 case S::ShiftCurrentFirst:{bit_=currentShift_&1;currentShift_>>=1;next();break;}
 case S::AppendCurrentFirst:{transition_=uint8_t((transition_<<1)|bit_);next();break;}
 case S::ShiftCurrentSecond:{bit_=currentShift_&1;currentShift_>>=1;next();break;}
 case S::AppendCurrentSecond:{transition_=uint8_t((transition_<<1)|bit_);next();break;}
 case S::ShiftPreviousFirst:{bit_=previousShift_&1;previousShift_>>=1;next();break;}
 case S::AppendPreviousFirst:{transition_=uint8_t((transition_<<1)|bit_);next();break;}
 case S::ShiftPreviousSecond:{bit_=previousShift_&1;previousShift_>>=1;next();break;}
 case S::AppendPreviousSecond:{transition_=uint8_t((transition_<<1)|bit_);next();break;}
 case S::ScaleTransition:{next();break;}
 case S::BindTransition:{next();break;}
 case S::DispatchTransition:{go(S::Transition);break;}
 case S::NextTransitionRecord:{if(++quad_<4)go(S::ClearTransition);else go(S::RestoreTransitionIndex);break;}
 case S::RestoreTransitionIndex:{next();break;}
 case S::ReturnChanges:{go(S::ResumeScan);break;}
 case S::Transition:{switch(transition_){case 1:go(S::ReadRepeatFlags);break;case 3:go(S::ReadFixedOffKey);break;case 4:go(S::ReadPressTick);break;case 7:go(S::ReadReleaseTick);break;case 12:go(S::ReadFixedOnKey);break;case 13:go(S::ReadReleaseFlags);break;default:go(S::NextTransitionRecord);break;}break;}
 case S::ReadPressTick:{eventTick_=uint16_t(read(R::EventTick,0xfffe86));next();break;}
 case S::StorePressTick:{write(W::RecordTick,record()+2,eventTick_);next();break;}
 case S::SetPressed:{recordFlags_=uint8_t(read(R::RecordFlagsKeep,record()))|2;write(W::RecordFlags,record(),recordFlags_);next();break;}
 case S::ReturnPress:{go(S::NextTransitionRecord);break;}
 case S::ReadReleaseTick:{eventTick_=uint16_t(read(R::EventTick,0xfffe86));next();break;}
 case S::StoreReleaseTick:{write(W::RecordTick,record()+2,eventTick_);next();break;}
 case S::ClearPressed:{recordFlags_=uint8_t(read(R::RecordFlagsKeep,record()))&uint8_t(~2);write(W::RecordFlags,record(),recordFlags_);next();break;}
 case S::ReturnRelease:{go(S::NextTransitionRecord);break;}
 case S::ReadFixedOnKey:{key_=uint8_t(read(R::EventKey,record()+1));next();break;}
 case S::BindFixedOnSpeed:{speed_=127;next();break;}
 case S::AppendFixedOn:{go(S::BiasEvent);break;}
 case S::ReadFixedOffKey:{key_=uint8_t(read(R::EventKey,record()+1));next();break;}
 case S::BindFixedOffSpeed:{speed_=255;next();break;}
 case S::AppendFixedOff:{go(S::BiasEvent);break;}
 case S::ReadRepeatFlags:{recordFlags_=uint8_t(read(R::RecordFlagsTest,record()));next();break;}
 case S::ChooseRepeat:{go(recordFlags_&2?S::ReturnRepeat:S::ReadRepeatTick);break;}
 case S::ReadRepeatTick:{eventTick_=uint16_t(read(R::EventTick,0xfffe86));next();break;}
 case S::ReadRepeatStart:{start_=uint16_t(read(R::EventStart,record()+2));next();break;}
 case S::ElapsedRepeat:{delay_=uint16_t(eventTick_-start_);next();break;}
 case S::BiasRepeat:{early_=delay_<10;delay_=uint16_t(delay_-10);next();break;}
 case S::ChooseEarlyRepeat:{go(early_?S::ReadFixedOffKey:S::IncrementRepeat);break;}
 case S::IncrementRepeat:{++delay_;next();break;}
 case S::CalculateRepeat:{repeatSpeed_=true;go(S::SaveSpeedValue);break;}
 case S::MarkRepeat:{speed_|=128;next();break;}
 case S::ReadRepeatKey:{key_=uint8_t(read(R::EventKey,record()+1));next();break;}
 case S::AppendRepeat:{go(S::BiasEvent);break;}
 case S::ReturnRepeat:{go(S::NextTransitionRecord);break;}
 case S::ReadReleaseFlags:{recordFlags_=uint8_t(read(R::RecordFlagsTest,record()));next();break;}
 case S::ChooseRelease:{go(recordFlags_&2?S::ReadVelocityTick:S::ReturnReleaseSpeed);break;}
 case S::ReadVelocityTick:{eventTick_=uint16_t(read(R::EventTick,0xfffe86));next();break;}
 case S::ReadVelocityStart:{start_=uint16_t(read(R::EventStart,record()+2));next();break;}
 case S::ElapsedVelocity:{delay_=uint16_t(eventTick_-start_);next();break;}
 case S::CalculateRelease:{repeatSpeed_=false;go(S::SaveSpeedValue);break;}
 case S::ReadReleaseKey:{key_=uint8_t(read(R::EventKey,record()+1));next();break;}
 case S::AppendRelease:{go(S::BiasEvent);break;}
 case S::ReturnReleaseSpeed:{go(S::NextTransitionRecord);break;}
 case S::SaveSpeedValue:{next();break;}
 case S::SaveSpeedIndex:{next();break;}
 case S::RetainDelay:{next();break;}
 case S::BindSlowPeriod:{period_=120;next();break;}
 case S::BindSlowBias:{bias_=3;next();break;}
 case S::ReadSpeedFlags:{recordFlags_=uint8_t(read(R::SpeedFlags,record()));next();break;}
 case S::ChooseSpeedMode:{go(recordFlags_&1?S::BindFastPeriod:S::SubtractDelayBias);break;}
 case S::BindFastPeriod:{period_=30;next();break;}
 case S::BindFastBias:{bias_=2;next();break;}
 case S::SubtractDelayBias:{delay_=uint16_t(delay_-bias_);next();break;}
 case S::ChooseDelayFloor:{go(delay_&0x8000?S::ClearDelayFloor:S::ClearNumerator);break;}
 case S::ClearDelayFloor:{delay_=0;next();break;}
 case S::ClearNumerator:{numerator_=uint32_t(period_)<<16;next();break;}
 case S::SquareDelay:{denominator_=uint32_t(delay_)*delay_;next();break;}
 case S::BindDenominatorBias:{next();break;}
 case S::NormalizeDenominatorBias:{next();break;}
 case S::AddDenominatorBias:{denominator_+=period_;next();break;}
 case S::HalfNumerator:{numerator_>>=1;next();break;}
 case S::TestDenominatorHigh:{next();break;}
 case S::ChooseDenominatorShift:{go(denominator_>>16?S::ShiftDenominator:S::DivideSpeed);break;}
 case S::ShiftDenominator:{denominator_>>=1;next();break;}
 case S::ShiftNumerator:{numerator_>>=1;next();break;}
 case S::RepeatDenominator:{go(S::TestDenominatorHigh);break;}
 case S::DivideSpeed:{quotient_=uint16_t(uint16_t(denominator_)?numerator_/uint16_t(denominator_):0);next();break;}
 case S::IncrementSpeed:{negativeSpeed_=int(int8_t(quotient_>>8))+1<0;speed_=uint8_t((quotient_>>8)+1);next();break;}
 case S::ChooseSpeedLimit:{go(negativeSpeed_?S::ClampSpeed:S::RestoreSpeedIndex);break;}
 case S::ClampSpeed:{speed_=127;next();break;}
 case S::RestoreSpeedIndex:{next();break;}
 case S::RestoreSpeedValue:{next();break;}
 case S::ReturnSpeed:{go(repeatSpeed_?S::MarkRepeat:S::ReadReleaseKey);break;}
 case S::BiasEvent:{message_=uint16_t((unsigned(speed_)<<8)|uint8_t(key_+12));next();break;}
 case S::SaveAppendIndex:{next();break;}
 case S::SaveAppendValue:{next();break;}
 case S::ReadProducer:{producer_=uint16_t(read(R::Producer,0xc6aa));next();break;}
 case S::NormalizeProducer:{next();break;}
 case S::BindQueue:{next();break;}
 case S::LocateEvent:{next();break;}
 case S::PublishEvent:{write(W::Event,queueRef(),message_);next();break;}
 case S::IncrementProducer:{nextProducer_=uint16_t(producer_+2);next();break;}
 case S::MaskProducer:{nextProducer_&=0x1fe;next();break;}
 case S::PublishProducer:{write(W::Producer,0xc6aa,nextProducer_);next();break;}
 case S::RestoreAppendValue:{next();break;}
 case S::RestoreAppendIndex:{next();break;}
 case S::ReturnAppend:{go(S::NextTransitionRecord);break;}
 case S::ReadPanelCount:{panelCount_=uint8_t(read(R::PanelCount,0xfffe8f+panel_));next();break;}
 case S::ChoosePanelActive:{if(panelCount_)next();else advancePanel();break;}
 case S::DecayPanel:{--panelCount_;next();break;}
 case S::PublishPanelCount:{write(W::PanelCount,0xfffe8f+panel_,panelCount_);next();break;}
 case S::ChoosePanelExpiry:{if(panelCount_)advancePanel();else go(panel_==2?S::RetainPanelCommand:S::RetainPanelPort);break;}
 case S::RetainPanelPort:{next();break;}
 case S::ReadPanelPort:{panelPort_=uint8_t(read(R::PanelPort,0xffffd2));next();break;}
 case S::MaskPanelPort:{panelPort_&=panel_?0xbf:0xef;next();break;}
 case S::ReadPanelMirror:{panelMirror_=uint8_t(read(R::PanelMirror,0xfffe8e));next();break;}
 case S::MaskPanelMirror:{panelMirror_&=panel_?0x20:0x10;next();break;}
 case S::MergePanel:{panelPort_|=panelMirror_;next();break;}
 case S::PublishPanelPort:{write(W::PanelPort,0xffffd2,panelPort_);next();break;}
 case S::RestorePanelPort:{advancePanel();break;}
 case S::RetainPanelCommand:{next();break;}
 case S::SetPanelCommand:{command_=12;next();break;}
 case S::DispatchPanelCommand:{go(S::RetainCommand);break;}
 case S::RestorePanelCommand:{go(S::ReturnPanel);break;}
 case S::ReturnPanel:{go(S::RestoreTimer);break;}
 case S::RetainCommand:{next();break;}
 case S::DispatchReadyPoll:{go(S::PollFirstRead);break;}
 case S::RestoreCommand:{next();break;}
 case S::DispatchCommandWrite:{go(S::RetainWrite);break;}
 case S::ReturnCommand:{go(S::RestorePanelCommand);break;}
 case S::PollFirstRead:{finalRead_=false;go(S::RetainRead);break;}
 case S::PollTest:{next();break;}
 case S::PollChoose:{go(lcdRead_&0x80?S::PollFirstRead:S::PollFinalRead);break;}
 case S::PollFinalRead:{finalRead_=true;go(S::RetainRead);break;}
 case S::PollReturn:{go(S::RestoreCommand);break;}
 case S::RetainRead:{next();break;}
 case S::ReadDirectionValue:{next();break;}
 case S::ReadDirection:{write(W::LcdLow,0xffffc5,0);next();break;}
 case S::ReadSelectValue:{next();break;}
 case S::ReadSelect:{write(W::LcdLow,0xffffd3,0x50);next();break;}
 case S::ReadEnableValue:{next();break;}
 case S::ReadEnable:{write(W::LcdLow,0xffffd3,0xd0);next();break;}
 case S::ReadWaitFirst:{next();break;}
 case S::ReadWaitSecond:{next();break;}
 case S::ReadWaitThird:{next();break;}
 case S::ReadLcdData:{lcdRead_=uint8_t(read(R::LcdData,0xffffc7));next();break;}
 case S::ReadFinishValue:{next();break;}
 case S::ReadFinish:{write(W::LcdLow,0xffffd3,0x50);next();break;}
 case S::ReadRestoreDirectionValue:{next();break;}
 case S::ReadRestoreDirection:{write(W::LcdLow,0xffffc5,255);next();break;}
 case S::RestoreRead:{next();break;}
 case S::ReturnRead:{go(finalRead_?S::PollReturn:S::PollTest);break;}
 case S::RetainWrite:{next();break;}
 case S::WriteSelectValue:{next();break;}
 case S::WriteSelect:{write(W::LcdLow,0xffffd3,0x40);next();break;}
 case S::WriteEnableValue:{next();break;}
 case S::WriteEnable:{write(W::LcdLow,0xffffd3,0xc0);next();break;}
 case S::WriteLcdData:{write(W::LcdCommand,0xffffc7,command_);next();break;}
 case S::WriteWaitFirst:{next();break;}
 case S::WriteWaitSecond:{next();break;}
 case S::WriteWaitThird:{next();break;}
 case S::WriteFinishValue:{next();break;}
 case S::WriteFinish:{write(W::LcdLow,0xffffd3,0x40);next();break;}
 case S::RestoreWrite:{next();break;}
 case S::ReturnWrite:{go(S::ReturnCommand);break;}
 case S::Complete:break;}return cost;}
private:
 void go(S s){stage_=s;}void next(){stage_=S(unsigned(stage_)+1);}void advancePage(){if(++page_<8)go(S::ReadPins);else go(S::RestoreScanRecord);}void advancePanel(){if(++panel_<3)go(S::ReadPanelCount);else go(S::ReturnPanel);}
 S stage_=S::RetainTimer;uint32_t tick_=0,numerator_=0,denominator_=0;uint16_t scanTick_=0,eventTick_=0,start_=0,delay_=0,period_=0,quotient_=0,message_=0,producer_=0,nextProducer_=0;
 uint8_t status_=0,divider_=0,tickTail_=0,page_=0,quad_=0,sample_=0,previous_=0,mux_=0,currentShift_=0,previousShift_=0,transition_=0,bit_=0,recordFlags_=0,key_=0,speed_=0,bias_=0,panel_=0,panelCount_=0,panelPort_=0,panelMirror_=0,command_=0,lcdRead_=0;
 bool awaitClock_=false,dividerCarry_=false,early_=false,repeatSpeed_=false,negativeSpeed_=false,finalRead_=false;
};
}
