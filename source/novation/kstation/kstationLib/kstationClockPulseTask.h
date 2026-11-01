#pragma once
#include <cstdint>
namespace kstation {
// Adapted from KS ClockPulseTask to K OS2.1.01 values and timing.
// External pulse feedback retains 24-bit signed phase differences, a wrapping
// sample count, original packed divisions and the adaptive integral/gain loop.
class ClockPulseTask final {
public:
 enum class Kind { Latched, Edge };enum class Stage { BindLatchedIndex, ReadLatchedSource, ConsumeLatched, ClearLatched, WriteLatched, ReturnLatched, SelectFlags, WriteSourceFlag, ReadActive, SelectActive, CompareResetSource, SelectResetSource, ReadResetTick, WriteResetTick, ClearResetCount, WriteResetCount, WriteResetSum, ReadResetPulses, WriteResetPulses, FinishReset, ReadPulseCount, IncrementPulseCount, WritePulseCount, ClearPulseFraction, ReadTimer, DifferenceTimer, CaptureTimerSign, ExtendTimerSign, BindTimerSign, ReadSum, AccumulateTimer, WriteSum, ReadSamples, IncrementSamples, WriteSamples, ReadTick, RetainTick, ReadPreviousTick, ElapsedTick, CompareElapsed, SelectElapsed, WritePreviousTick, ClearSampleCount, WriteSampleCount, WriteSumHigh, WriteSumLow, DivideAverage, RetainAverage, BindRateFactor, MultiplyRate, DivideRate, ReadEstimate, DifferenceRate, RetainError, ReadGain, MultiplyCorrection, DoubleCorrection, AddEstimate, AddSecondCorrection, WriteEstimate, QuarterEstimate, QuarterEstimateAgain, RestoreAverage, ClearAverageLow, QuarterAverage, QuarterAverageAgain, DifferencePeriod, WritePeriod, ReadIntegral, AddIntegralError, ReadSaturationGain, CompareSaturationGain, SelectSaturationGain, ClearIntegral, BindIntegralFactor, MultiplyIntegral, DoubleIntegral, WriteIntegral, SelectIntegralSign, NegateIntegralMagnitude, CompareIntegralThreshold, SelectIntegralThreshold, SetMaximumGain, FinishMaximumGain, ReadDecayGain, DecayGain, SelectGainSign, CompareGainFloor, SelectGainFloor, SetMinimumGain, WriteGain, ReturnPulse, AwaitChild, Complete };
 enum class Read { Latched, Flags, Active, Pulses, PulseCount, Timer, Sum, Samples, PreviousTick, Estimate, Gain, Integral };enum class Write { Latched, Flags, PreviousTick, Samples, Sum, PulseCount, SumHigh, SumLow, Estimate, Period, Integral, Gain };
 ClockPulseTask(Kind kind,uint8_t index,uint8_t source):stage_(kind==Kind::Latched?Stage::BindLatchedIndex:Stage::SelectFlags),index_(index),source_(source){}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint16_t tick){if(!awaitingChild())return false;tick_=tick;stage_=resume_;return true;}
 uint8_t source()const{return source_;}uint8_t active()const{return active_;}uint16_t pulses()const{return pulses_;}uint16_t samples()const{return samples_;}uint16_t baseline()const{return baseline_;}uint32_t timer()const{return timer_;}uint32_t sum()const{return sum_;}uint32_t estimate()const{return estimate_;}uint16_t gain()const{return gain_;}uint16_t integral()const{return integral_;}
 struct Credits {unsigned call=10,shortCall=5,ret=7,pushWord=6,popWord=6,flagsWrite=1;};
 unsigned nextClocks(const Credits&c)const{switch(stage_){
 case Stage::IncrementSamples:case Stage::CompareElapsed:case Stage::BindRateFactor:case Stage::CompareSaturationGain:case Stage::BindIntegralFactor:case Stage::CompareIntegralThreshold:case Stage::SetMaximumGain:case Stage::DecayGain:case Stage::CompareGainFloor:case Stage::SetMinimumGain:return 6;
 case Stage::FinishReset:case Stage::SelectElapsed:return 8;
 case Stage::SelectFlags:return 9;
 case Stage::RetainAverage:return c.pushWord;case Stage::RestoreAverage:return c.popWord;
 case Stage::ReadLatchedSource:case Stage::WriteLatched:case Stage::ReadActive:return 10;
 case Stage::WriteSourceFlag:return 7+c.flagsWrite;
 case Stage::WriteResetTick:case Stage::WriteResetCount:case Stage::ReadResetPulses:case Stage::WriteResetPulses:case Stage::ReadPulseCount:case Stage::WritePulseCount:case Stage::ReadSamples:case Stage::WriteSamples:case Stage::ReadPreviousTick:case Stage::WritePreviousTick:case Stage::WriteSampleCount:case Stage::WriteSumHigh:case Stage::WriteSumLow:case Stage::ReadGain:case Stage::ReadIntegral:case Stage::ReadSaturationGain:case Stage::WriteIntegral:case Stage::ReadDecayGain:case Stage::WriteGain:return 10;
 case Stage::ReturnLatched:case Stage::ReturnPulse:return c.ret;
 case Stage::ConsumeLatched:return c.shortCall;case Stage::ReadResetTick:case Stage::ReadTick:return c.call;
 case Stage::MultiplyRate:case Stage::DivideRate:return 23;
 case Stage::WriteResetSum:case Stage::ReadTimer:case Stage::ReadSum:case Stage::WriteSum:case Stage::ReadEstimate:case Stage::WriteEstimate:case Stage::WritePeriod:return 14;
 case Stage::DivideAverage:case Stage::MultiplyCorrection:case Stage::MultiplyIntegral:return 26;
 case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits&credits,Reader&& read,Writer&& write){const auto cost=nextClocks(credits);if(!cost||cost>available)return 0;switch(stage_){case Stage::ConsumeLatched:await(Stage::ClearLatched);break;
case Stage::ReturnLatched:stage_=Stage::Complete;break;
case Stage::ReadResetTick:await(Stage::WriteResetTick);break;
case Stage::FinishReset:stage_=Stage::ReturnPulse;break;
case Stage::ReadTick:await(Stage::RetainTick);break;
case Stage::FinishMaximumGain:stage_=Stage::WriteGain;break;
case Stage::ReturnPulse:stage_=Stage::Complete;break;
case Stage::BindLatchedIndex:index_=0;next();break;
case Stage::ReadLatchedSource:source_=uint8_t(read(Read::Latched));next();break;
case Stage::WriteLatched:write(Write::Latched,0);next();break;
case Stage::WriteSourceFlag:write(Write::Flags,uint8_t(read(Read::Flags))|(1u<<(index_&7)));next();break;
case Stage::ReadActive:active_=uint8_t(read(Read::Active));next();break;
case Stage::SelectActive:stage_=active_?Stage::CompareResetSource:Stage::ReadResetTick;break;
case Stage::SelectResetSource:stage_=source_==255?Stage::ReadResetTick:Stage::ReadPulseCount;break;
case Stage::WriteResetTick:write(Write::PreviousTick,tick_);next();break;
case Stage::WriteResetCount:write(Write::Samples,0);next();break;
case Stage::WriteResetSum:write(Write::Sum,0);next();break;
case Stage::ReadResetPulses:pulses_=uint16_t(read(Read::Pulses));next();break;
case Stage::WriteResetPulses:write(Write::PulseCount,pulses_);next();break;
case Stage::ReadPulseCount:pulses_=uint16_t(read(Read::PulseCount));next();break;
case Stage::IncrementPulseCount:++pulses_;next();break;
case Stage::WritePulseCount:write(Write::PulseCount,pulses_);next();break;
case Stage::ReadTimer:timer_=read(Read::Timer);next();break;
case Stage::DifferenceTimer:difference_=timer_-(uint32_t(pulses_)<<16);next();break;
case Stage::ExtendTimerSign:difference_=(difference_&0x800000u)?difference_|0xff000000u:difference_&0xffffffu;next();break;
case Stage::ReadSum:sum_=read(Read::Sum);next();break;
case Stage::AccumulateTimer:sum_+=difference_;next();break;
case Stage::WriteSum:write(Write::Sum,sum_);next();break;
case Stage::ReadSamples:samples_=uint16_t(read(Read::Samples));next();break;
case Stage::IncrementSamples:samples_=uint16_t(samples_+512);next();break;
case Stage::WriteSamples:write(Write::Samples,samples_);next();break;
case Stage::ReadPreviousTick:baseline_=uint16_t(read(Read::PreviousTick));next();break;
case Stage::ElapsedTick:elapsed_=uint16_t(tick_-baseline_);next();break;
case Stage::SelectElapsed:stage_=elapsed_<20000?Stage::ReturnPulse:Stage::WritePreviousTick;break;
case Stage::WritePreviousTick:write(Write::PreviousTick,tick_);next();break;
case Stage::WriteSampleCount:write(Write::Samples,0);next();break;
case Stage::WriteSumHigh:write(Write::SumHigh,0);next();break;
case Stage::WriteSumLow:write(Write::SumLow,0);next();break;
case Stage::DivideAverage:average_=uint16_t(divideSigned(sum_,samples_));next();break;
case Stage::MultiplyRate:packed_=uint32_t(samples_)*64000u;next();break;
case Stage::DivideRate:packed_=divideUnsigned(packed_,elapsed_);next();break;
case Stage::ReadEstimate:estimate_=read(Read::Estimate);next();break;
case Stage::DifferenceRate:error_=uint16_t(uint16_t(packed_)-uint16_t(estimate_>>16));next();break;
case Stage::ReadGain:gain_=uint16_t(read(Read::Gain));next();break;
case Stage::MultiplyCorrection:correction_=uint32_t(int32_t(int16_t(error_))*int32_t(int16_t(gain_)));next();break;
case Stage::DoubleCorrection:correction_<<=1;next();break;
case Stage::AddEstimate:estimate_+=correction_;next();break;
case Stage::AddSecondCorrection:correction_+=estimate_;next();break;
case Stage::WriteEstimate:write(Write::Estimate,correction_);next();break;
case Stage::QuarterEstimate:filtered_=correction_>>2;next();break;
case Stage::QuarterAverage:averageCorrection_=uint32_t(int32_t(int16_t(average_))*16384);next();break;
case Stage::DifferencePeriod:period_=filtered_-averageCorrection_;next();break;
case Stage::WritePeriod:write(Write::Period,period_);next();break;
case Stage::ReadIntegral:integral_=uint16_t(read(Read::Integral));next();break;
case Stage::AddIntegralError:integral_=uint16_t(integral_+error_);next();break;
case Stage::ReadSaturationGain:gain_=uint16_t(read(Read::Gain));next();break;
case Stage::SelectSaturationGain:stage_=gain_==0x4000?Stage::ClearIntegral:Stage::BindIntegralFactor;break;
case Stage::ClearIntegral:integral_=0;next();break;
case Stage::MultiplyIntegral:integralProduct_=uint32_t(int32_t(int16_t(integral_))*0x7e00);next();break;
case Stage::DoubleIntegral:integralProduct_<<=1;next();break;
case Stage::WriteIntegral:magnitude_=uint16_t(integralProduct_>>16);write(Write::Integral,magnitude_);next();break;
case Stage::SelectIntegralSign:stage_=(integralProduct_&0x80000000u)?Stage::NegateIntegralMagnitude:Stage::CompareIntegralThreshold;break;
case Stage::NegateIntegralMagnitude:magnitude_=uint16_t(0u-magnitude_);next();break;
case Stage::SelectIntegralThreshold:stage_=magnitude_<=0x1800?Stage::ReadDecayGain:Stage::SetMaximumGain;break;
case Stage::SetMaximumGain:gain_=0x4000;next();break;
case Stage::ReadDecayGain:gain_=uint16_t(read(Read::Gain));next();break;
case Stage::DecayGain:gain_=uint16_t(gain_-0x800);next();break;
case Stage::SelectGainSign:stage_=(gain_&0x8000)?Stage::SetMinimumGain:Stage::CompareGainFloor;break;
case Stage::SelectGainFloor:stage_=gain_<0x80?Stage::SetMinimumGain:Stage::WriteGain;break;
case Stage::SetMinimumGain:gain_=0x80;next();break;
case Stage::WriteGain:write(Write::Gain,gain_);next();break; default:next();break;}return cost;}
private:
 static uint32_t divideUnsigned(uint32_t numerator,uint16_t divisor){const uint32_t quotient=divisor?numerator/divisor:0;return ((numerator-quotient*divisor)<<16)|uint16_t(quotient);}
 static uint32_t divideSigned(uint32_t numerator,uint16_t divisor){const auto n=int32_t(numerator);const auto d=int16_t(divisor);if(!d&&n)return numerator;const int64_t q=d?int64_t(n)/d:1,rem=int64_t(n)-q*d;return (uint32_t(rem)<<16)|uint16_t(q);}
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_,resume_=Stage::Complete;uint8_t index_,source_,active_=0;uint16_t pulses_=0,samples_=0,tick_=0,baseline_=0,elapsed_=0,average_=0,error_=0,gain_=0,integral_=0,magnitude_=0;uint32_t timer_=0,difference_=0,sum_=0,packed_=0,estimate_=0,correction_=0,filtered_=0,averageCorrection_=0,period_=0,integralProduct_=0;
};
}
