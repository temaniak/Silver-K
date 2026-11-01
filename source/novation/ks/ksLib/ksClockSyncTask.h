#pragma once
#include <cstdint>
namespace ks {
// Internal/external clock coefficient service. Normalization, packed two-step
// division, wrapping feedback and separate DSP publications retain OS timing.
class ClockSyncTask final {
public:
 enum class Kind { Update };enum class Stage { ReadDivider, ToggleDivider, WriteDivider, TestDivider, SelectDivider, ReturnIdle, SelectFlags, SelectGlobals, ReadSource, SelectSource, ClearSourceFlag, ReadSourceFlags, BindSourceFlags, DecrementSource, CaptureSourceFlag, WriteSourceFlag, MaskActive, WriteActive, SelectInternal, ReadInternalRate, BiasInternalRate, NormalizeInternalRate, ScaleInternalFour, ScaleInternalSixteen, ScaleInternalThirtyTwo, BindInternalWeight, MultiplyInternalRate, WriteInternalPeriod, ReadPeriod, ScalePeriod, ReadActive, SelectExternal, ReadSignal, ReadSynchronized, ComputeSignalError, ReadGain, ComputeCorrection, DoubleCorrection, AddCorrection, AddSecondCorrection, WriteSynchronized, BindResolution, SetResolutionPasses, TestResolution, SelectResolution, DoubleRate, DoubleResolution, DecrementResolutionPasses, MoreResolution, DivideHigh, RetainHigh, ClearLow, DivideLow, CombineCoefficient, ReadActiveAgain, SelectGain, ReadGainAgain, CompareMinimumGain, SelectMinimumGain, CompareTransitionGain, SelectTransitionGain, DecrementTransitionGain, WriteGain, FinishGain, ReadPreviousCoefficient, DifferenceCoefficient, SelectCoefficientSign, NegateCoefficientDifference, CompareCoefficientThreshold, SelectCoefficientThreshold, WriteCoefficient, SelectCoefficientWord, SendCoefficient, ReadPublishPeriod, QuantizePeriod, ReadPreviousPeriod, ComparePeriod, SelectPeriod, WritePreviousPeriod, ScalePublishPeriod, SelectPeriodWord, RetainPeriodWord, RetainPeriodValue, SendPeriodAddress, RestorePeriodValue, RestorePeriodWord, SendPeriodValue, ReturnUpdate, AwaitChild, Complete };
 enum class Read { Divider, Source, Flags, InternalRate, Period, Active, Signal, Synchronized, Gain, Coefficient, PreviousPeriod };enum class Write { Divider, Flags, Active, Period, Synchronized, Gain, Coefficient, PreviousPeriod };
 explicit ClockSyncTask(Kind){}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 uint8_t divider()const{return divider_;}uint8_t source()const{return source_;}uint8_t internal()const{return internal_;}uint8_t active()const{return active_;}uint16_t signal()const{return signal_;}uint16_t gain()const{return gain_;}uint32_t rate()const{return rate_;}uint32_t period()const{return period_;}uint32_t previous()const{return previous_;}uint32_t coefficient()const{return coefficient_;}
 unsigned nextClocks()const{switch(stage_){case Stage::ReadSourceFlags:case Stage::BindInternalWeight:case Stage::CompareMinimumGain:case Stage::CompareTransitionGain:case Stage::SelectCoefficientWord:case Stage::SelectPeriodWord:return 6; case Stage::SelectFlags:case Stage::SelectGlobals:case Stage::BindResolution:case Stage::CompareCoefficientThreshold:case Stage::QuantizePeriod:return 9; case Stage::RetainPeriodWord:case Stage::RestorePeriodWord:return 11; case Stage::ReadDivider:case Stage::WriteDivider:case Stage::ClearSourceFlag:case Stage::WriteSourceFlag:case Stage::WriteActive:case Stage::ReadActive:case Stage::ReadActiveAgain:return 12; case Stage::ReadSource:case Stage::ReadInternalRate:case Stage::ReadSignal:case Stage::ReadGain:case Stage::ReadGainAgain:case Stage::WriteGain:return 15; case Stage::ReturnIdle:case Stage::ReturnUpdate:return 17; case Stage::SendCoefficient:case Stage::RetainPeriodValue:case Stage::SendPeriodAddress:case Stage::RestorePeriodValue:case Stage::SendPeriodValue:return 20; case Stage::MultiplyInternalRate:case Stage::DivideHigh:case Stage::DivideLow:return 23; case Stage::WriteInternalPeriod:case Stage::ReadPeriod:case Stage::ReadSynchronized:case Stage::WriteSynchronized:case Stage::ReadPreviousCoefficient:case Stage::WriteCoefficient:case Stage::ReadPublishPeriod:case Stage::ReadPreviousPeriod:case Stage::WritePreviousPeriod:return 24; case Stage::ComputeCorrection:return 26; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){const auto cost=nextClocks();if(!cost||cost>available)return 0;switch(stage_){case Stage::ReturnIdle:stage_=Stage::Complete;break;
case Stage::BindSourceFlags:stage_=Stage::MaskActive;break;
case Stage::FinishGain:stage_=Stage::ReadPublishPeriod;break;
case Stage::SendCoefficient:await(Stage::ReadPublishPeriod);break;
case Stage::SendPeriodAddress:await(Stage::RestorePeriodValue);break;
case Stage::SendPeriodValue:await(Stage::ReturnUpdate);break;
case Stage::ReturnUpdate:stage_=Stage::Complete;break;
case Stage::ReadDivider:divider_=uint8_t(read(Read::Divider));next();break;
case Stage::ToggleDivider:divider_^=1;next();break;
case Stage::WriteDivider:write(Write::Divider,divider_);next();break;
case Stage::SelectDivider:stage_=(divider_&1)?Stage::ReturnIdle:Stage::SelectFlags;break;
case Stage::ReadSource:source_=uint8_t(read(Read::Source));next();break;
case Stage::SelectSource:stage_=source_?Stage::DecrementSource:Stage::ClearSourceFlag;break;
case Stage::ClearSourceFlag:write(Write::Flags,uint8_t(read(Read::Flags))&~2u);next();break;
case Stage::ReadSourceFlags:source_=uint8_t(read(Read::Flags));next();break;
case Stage::DecrementSource:--source_;next();break;
case Stage::WriteSourceFlag:write(Write::Flags,(uint8_t(read(Read::Flags))&~2u)|(source_&2));next();break;
case Stage::MaskActive:active_=source_&1;next();break;
case Stage::WriteActive:write(Write::Active,active_);next();break;
case Stage::SelectInternal:stage_=active_?Stage::ReadPeriod:Stage::ReadInternalRate;break;
case Stage::ReadInternalRate:internal_=uint8_t(read(Read::InternalRate));next();break;
case Stage::BiasInternalRate:internal_=uint8_t(internal_+64);next();break;
case Stage::ScaleInternalThirtyTwo:rate_=uint32_t(internal_)<<5;next();break;
case Stage::MultiplyInternalRate:period_=rate_*0xd1b7u;next();break;
case Stage::WriteInternalPeriod:write(Write::Period,period_);next();break;
case Stage::ReadPeriod:rate_=read(Read::Period);next();break;
case Stage::ScalePeriod:rate_<<=2;next();break;
case Stage::ReadActive:active_=uint8_t(read(Read::Active));next();break;
case Stage::SelectExternal:stage_=active_?Stage::ReadSignal:Stage::BindResolution;break;
case Stage::ReadSignal:signal_=uint16_t(read(Read::Signal));next();break;
case Stage::ReadSynchronized:rate_=read(Read::Synchronized);next();break;
case Stage::ComputeSignalError:signal_=uint16_t(signal_-uint16_t(rate_>>16));next();break;
case Stage::ReadGain:gain_=uint16_t(read(Read::Gain));next();break;
case Stage::ComputeCorrection:correction_=uint32_t(int32_t(int16_t(signal_))*int32_t(int16_t(gain_)));next();break;
case Stage::DoubleCorrection:correction_<<=1;next();break;
case Stage::AddCorrection:rate_+=correction_;next();break;
case Stage::AddSecondCorrection:rate_+=correction_;next();break;
case Stage::WriteSynchronized:write(Write::Synchronized,rate_);next();break;
case Stage::BindResolution:resolution_=0x4000000;next();break;
case Stage::SetResolutionPasses:passes_=5;next();break;
case Stage::SelectResolution:stage_=(rate_&0x80000000u)?Stage::DivideHigh:Stage::DoubleRate;break;
case Stage::DoubleRate:rate_<<=1;next();break;
case Stage::DoubleResolution:resolution_<<=1;next();break;
case Stage::DecrementResolutionPasses:--passes_;next();break;
case Stage::MoreResolution:stage_=passes_?Stage::TestResolution:Stage::DivideHigh;break;
case Stage::DivideHigh:packed_=divide(resolution_,uint16_t(rate_>>16));next();break;
case Stage::RetainHigh:high_=uint16_t(packed_);next();break;
case Stage::ClearLow:packed_&=0xffff0000u;next();break;
case Stage::DivideLow:packed_=divide(packed_,uint16_t(rate_>>16));next();break;
case Stage::CombineCoefficient:coefficient_=(uint32_t(high_)<<16)|uint16_t(packed_);next();break;
case Stage::ReadActiveAgain:active_=uint8_t(read(Read::Active));next();break;
case Stage::SelectGain:stage_=active_?Stage::ReadGainAgain:Stage::ReadPreviousCoefficient;break;
case Stage::ReadGainAgain:gain_=uint16_t(read(Read::Gain));next();break;
case Stage::SelectMinimumGain:stage_=gain_==0x80?Stage::ReadPreviousCoefficient:Stage::CompareTransitionGain;break;
case Stage::SelectTransitionGain:stage_=gain_==0x800?Stage::DecrementTransitionGain:Stage::ReadPublishPeriod;break;
case Stage::DecrementTransitionGain:--gain_;next();break;
case Stage::WriteGain:write(Write::Gain,gain_);next();break;
case Stage::ReadPreviousCoefficient:previous_=read(Read::Coefficient);next();break;
case Stage::DifferenceCoefficient:difference_=previous_-coefficient_;next();break;
case Stage::SelectCoefficientSign:stage_=(difference_&0x80000000u)?Stage::NegateCoefficientDifference:Stage::CompareCoefficientThreshold;break;
case Stage::NegateCoefficientDifference:difference_=0u-difference_;next();break;
case Stage::SelectCoefficientThreshold:stage_=difference_<0x1000?Stage::ReadPublishPeriod:Stage::WriteCoefficient;break;
case Stage::WriteCoefficient:write(Write::Coefficient,coefficient_);next();break;
case Stage::ReadPublishPeriod:period_=read(Read::Period);next();break;
case Stage::QuantizePeriod:period_&=0xfffff000u;next();break;
case Stage::ReadPreviousPeriod:previous_=read(Read::PreviousPeriod);next();break;
case Stage::SelectPeriod:stage_=previous_==period_?Stage::ReturnUpdate:Stage::WritePreviousPeriod;break;
case Stage::WritePreviousPeriod:write(Write::PreviousPeriod,period_);next();break;
case Stage::ScalePublishPeriod:period_<<=2;next();break; default:next();break;}return cost;}
private:
 static uint32_t divide(uint32_t numerator,uint16_t divisor){const uint32_t quotient=divisor?numerator/divisor:0;return ((numerator-quotient*divisor)<<16)|uint16_t(quotient);}
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::ReadDivider,resume_=Stage::Complete;uint8_t divider_=0,source_=0,internal_=0,active_=0;uint16_t signal_=0,gain_=0,high_=0;unsigned passes_=0;uint32_t rate_=0,period_=0,correction_=0,resolution_=0,packed_=0,coefficient_=0,previous_=0,difference_=0;
};
}
