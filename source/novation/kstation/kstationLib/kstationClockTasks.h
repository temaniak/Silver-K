#pragma once
#include <cstdint>
namespace kstation {
// KS-style timed domain tasks specialized for K's on-chip clock resources.
struct ClockCredits {unsigned retainLong=10,restoreLong=10,retainWord=6,restoreWord=6,call=10,shortCall=5,ret=7,flagWrite=1,latchWrite=1;};
class ClockTickTask final {
public:
 enum class Stage {RetainIncrement,RetainCount,ReadIncrement,ReadPhase,ReadPulseCount,AdvancePhase,SelectCarry,IncrementPulseCount,ReadOutputFlags,TestOutputFlag,SelectOutput,SendClock,WritePhase,WritePulseCount,ReadSecondary,ReadSecondaryIncrement,AdvanceSecondary,WriteSecondary,ReadRunFlags,ReadTimeout,AdvanceTimeout,SelectTimeout,WriteTimeout,SkipTimeoutClear,ClearTimeoutFlag,WriteRunFlags,RestoreCount,RestoreIncrement,Return,AwaitOutput,Complete};
 enum class Read {Increment,Phase,PulseCount,OutputFlags,Secondary,SecondaryIncrement,RunFlags,Timeout};enum class Write {Phase,PulseCount,Secondary,Timeout,RunFlags};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingChild()const{return stage_==Stage::AwaitOutput;}bool resumeChild(){if(!awaitingChild())return false;stage_=Stage::WritePhase;return true;}
 unsigned nextClocks(const ClockCredits&c)const{switch(stage_){case Stage::RetainIncrement:case Stage::RetainCount:return c.retainLong;case Stage::RestoreCount:case Stage::RestoreIncrement:return c.restoreLong;case Stage::Return:return c.ret;case Stage::SendClock:return c.call;case Stage::ReadIncrement:case Stage::ReadPhase:case Stage::ReadSecondary:case Stage::ReadSecondaryIncrement:case Stage::WritePhase:case Stage::WriteSecondary:return 14;case Stage::ReadPulseCount:case Stage::WritePulseCount:case Stage::ReadOutputFlags:case Stage::ReadRunFlags:case Stage::ReadTimeout:case Stage::WriteTimeout:return 10;case Stage::WriteRunFlags:return 9+c.flagWrite;case Stage::AwaitOutput:case Stage::Complete:return 0;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,const ClockCredits&c,R&&read,W&&write){const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;switch(stage_){
 case Stage::ReadIncrement:increment_=read(Read::Increment);next();break;case Stage::ReadPhase:phase_=read(Read::Phase);next();break;case Stage::ReadPulseCount:pulses_=uint16_t(read(Read::PulseCount));next();break;
 case Stage::AdvancePhase:{const uint64_t sum=uint64_t(phase_)+increment_;carry_=sum>0xffffffffu;phase_=uint32_t(sum);next();break;}
 case Stage::SelectCarry:stage_=carry_?Stage::IncrementPulseCount:Stage::WritePhase;break;case Stage::IncrementPulseCount:++pulses_;next();break;
 case Stage::ReadOutputFlags:output_=uint8_t(read(Read::OutputFlags));next();break;case Stage::SelectOutput:stage_=(output_&2)?Stage::SendClock:Stage::WritePhase;break;
 case Stage::SendClock:stage_=Stage::AwaitOutput;break;
 case Stage::WritePhase:write(Write::Phase,phase_);next();break;case Stage::WritePulseCount:write(Write::PulseCount,pulses_);next();break;
 case Stage::ReadSecondary:secondary_=read(Read::Secondary);next();break;case Stage::ReadSecondaryIncrement:secondaryIncrement_=read(Read::SecondaryIncrement);next();break;case Stage::AdvanceSecondary:secondary_+=secondaryIncrement_;next();break;case Stage::WriteSecondary:write(Write::Secondary,secondary_);next();break;
 case Stage::ReadRunFlags:run_=uint8_t(read(Read::RunFlags));next();break;case Stage::ReadTimeout:timeout_=uint8_t(read(Read::Timeout));next();break;case Stage::AdvanceTimeout:++timeout_;next();break;case Stage::SelectTimeout:stage_=timeout_?Stage::WriteTimeout:Stage::ClearTimeoutFlag;break;case Stage::WriteTimeout:write(Write::Timeout,timeout_);next();break;case Stage::SkipTimeoutClear:stage_=Stage::WriteRunFlags;break;case Stage::ClearTimeoutFlag:run_&=0xfe;next();break;case Stage::WriteRunFlags:write(Write::RunFlags,run_);next();break;case Stage::Return:stage_=Stage::Complete;break;default:next();break;}return cost;}
private:void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_=Stage::RetainIncrement;uint32_t phase_=0,increment_=0,secondary_=0,secondaryIncrement_=0;uint16_t pulses_=0;uint8_t output_=0,run_=0,timeout_=0;bool carry_=false;
};
class ClockReadTask final {
public:
 enum class Stage {ReadCounter,ReadOverflow,CompareCounter,SelectCounter,ReadFlag,SelectFlag,IncrementOverflow,ScaleFirst,ScaleSecond,ScaleThird,ScaleFourth,Return,Complete};
 enum class Read {Counter,Overflow,Flag};Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}uint16_t result()const{return uint16_t(ticks_);}
 unsigned nextClocks(const ClockCredits&c)const{switch(stage_){case Stage::ReadCounter:case Stage::ReadFlag:return 7;case Stage::ReadOverflow:return 10;case Stage::CompareCounter:return 6;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned budget,const ClockCredits&c,R&&read){const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;switch(stage_){case Stage::ReadCounter:counter_=uint16_t(read(Read::Counter));next();break;case Stage::ReadOverflow:overflow_=uint16_t(read(Read::Overflow));ticks_=(uint32_t(overflow_)<<16)|counter_;next();break;case Stage::SelectCounter:stage_=counter_>1000?Stage::ScaleFirst:Stage::ReadFlag;break;case Stage::ReadFlag:flag_=uint8_t(read(Read::Flag));next();break;case Stage::SelectFlag:stage_=(flag_&4)?Stage::IncrementOverflow:Stage::ScaleFirst;break;case Stage::IncrementOverflow:++overflow_;ticks_=(uint32_t(overflow_)<<16)|counter_;next();break;case Stage::ScaleFirst:case Stage::ScaleSecond:case Stage::ScaleThird:case Stage::ScaleFourth:ticks_>>=1;next();break;case Stage::Return:stage_=Stage::Complete;break;default:next();break;}return cost;}
private:void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_=Stage::ReadCounter;uint16_t counter_=0,overflow_=0;uint8_t flag_=0;uint32_t ticks_=0;
};
class ClockSendTask final {
public:
 enum class Stage {RetainValue,PrepareByte,PublishByte,RetainPriority,MaskPriority,PrepareEnable,Enable,RestorePriority,RestoreValue,Return,Complete};enum class Write{Latch,Control};Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks(const ClockCredits&c)const{switch(stage_){case Stage::RetainValue:return c.retainWord;case Stage::PublishByte:return 9+c.latchWrite;case Stage::Enable:return 4;case Stage::RestoreValue:return c.restoreWord;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class W>unsigned advanceBefore(unsigned budget,const ClockCredits&c,W&&write){const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;if(stage_==Stage::PublishByte)write(Write::Latch,0xf8);else if(stage_==Stage::Enable)write(Write::Control,0xf0);stage_=Stage(unsigned(stage_)+1);return cost;}
private:Stage stage_=Stage::RetainValue;
};
}
