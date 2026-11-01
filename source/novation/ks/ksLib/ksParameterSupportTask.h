#pragma once
#include <cstdint>
namespace ks {
class ParameterSupportTask final {
public:
 enum class Kind {Rate,Sync,Clock,Exit};enum class Stage {ReadRate,ClearRateLow,DoubleRate,SetRateScale,MultiplyRate,DispatchRate,ReadClock,ReadClockFlags,MaskClockFlags,SubtractFlags,OffsetClock,SetClockMultiplier,MultiplyClock,DispatchCoefficient,CopyCoefficient,DispatchClock,ReadSync,ClearSyncHigh,RetainSync,DispatchSyncFirst,RestoreSync,ReadSyncFlags,TestSyncFlag,AdmitSyncFlag,OverrideSync,DispatchSyncLast,DispatchExitFirst,DispatchExitSecond,ReturnExit,AwaitRate,AwaitCoefficient,AwaitClock,AwaitSyncFirst,AwaitSyncLast,AwaitExitFirst,AwaitExitSecond,Complete};enum class Read {Rate,Clock,Flags,Sync,SyncFlags};
 explicit ParameterSupportTask(Kind k):kind_(k),phase_(k==Kind::Rate?Stage::ReadRate:k==Kind::Clock?Stage::ReadClock:k==Kind::Sync?Stage::ReadSync:Stage::DispatchExitFirst){}
 Kind kind()const{return kind_;}Stage stage()const{return phase_;}uint16_t scaleInput()const{return input_;}uint32_t dspWord()const{return output_>>8;}bool complete()const{return phase_==Stage::Complete;}
 bool terminalAwait()const{return phase_==Stage::AwaitRate||phase_==Stage::AwaitClock||phase_==Stage::AwaitSyncLast;}
 bool awaitingChild()const{return terminalAwait()||phase_==Stage::AwaitCoefficient||phase_==Stage::AwaitSyncFirst||phase_==Stage::AwaitExitFirst||phase_==Stage::AwaitExitSecond;}
 bool resumeChild(uint32_t result=0){switch(phase_){case Stage::AwaitRate:case Stage::AwaitClock:case Stage::AwaitSyncLast:phase_=Stage::Complete;return true;
 case Stage::AwaitCoefficient:output_=result;phase_=Stage::CopyCoefficient;return true;case Stage::AwaitSyncFirst:phase_=Stage::RestoreSync;return true;case Stage::AwaitExitFirst:phase_=Stage::DispatchExitSecond;return true;case Stage::AwaitExitSecond:phase_=Stage::ReturnExit;return true;default:return false;}}
 unsigned nextClocks()const{switch(phase_){case Stage::ReadRate:case Stage::ReadClock:case Stage::ReadClockFlags:case Stage::ReadSync:case Stage::ReadSyncFlags:case Stage::MultiplyClock:return 15;
 case Stage::SetRateScale:return 6;case Stage::MultiplyRate:return 23;case Stage::RetainSync:case Stage::RestoreSync:case Stage::DispatchRate:case Stage::DispatchCoefficient:case Stage::DispatchClock:case Stage::DispatchSyncFirst:case Stage::DispatchSyncLast:case Stage::DispatchExitFirst:case Stage::DispatchExitSecond:return 20;
 case Stage::ReturnExit:return 17;case Stage::AwaitRate:case Stage::AwaitCoefficient:case Stage::AwaitClock:case Stage::AwaitSyncFirst:case Stage::AwaitSyncLast:case Stage::AwaitExitFirst:case Stage::AwaitExitSecond:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;
 switch(phase_){case Stage::ReadRate:rate_=uint8_t(read(Read::Rate));next();break;case Stage::ClearRateLow:input_=uint16_t(rate_)<<8;next();break;case Stage::DoubleRate:input_=uint16_t(unsigned(input_)*2);next();break;case Stage::MultiplyRate:output_=uint32_t(input_)*0x8102;next();break;case Stage::DispatchRate:phase_=Stage::AwaitRate;break;
 case Stage::ReadClock:clock_=uint8_t(read(Read::Clock));next();break;case Stage::ReadClockFlags:flags_=uint8_t(read(Read::Flags));next();break;case Stage::MaskClockFlags:flags_&=7;next();break;case Stage::SubtractFlags:clock_=uint8_t(clock_-flags_);next();break;case Stage::OffsetClock:clock_=uint8_t(clock_-94);next();break;case Stage::MultiplyClock:input_=uint16_t(unsigned(clock_)*170);next();break;case Stage::DispatchCoefficient:phase_=Stage::AwaitCoefficient;break;case Stage::DispatchClock:phase_=Stage::AwaitClock;break;
 case Stage::ReadSync:sync_=uint8_t(read(Read::Sync));output_=uint32_t(sync_)<<8;next();break;case Stage::DispatchSyncFirst:phase_=Stage::AwaitSyncFirst;break;case Stage::ReadSyncFlags:flags_=uint8_t(read(Read::SyncFlags));next();break;case Stage::AdmitSyncFlag:phase_=flags_&32?Stage::DispatchSyncLast:Stage::OverrideSync;break;case Stage::OverrideSync:output_=3<<8;next();break;case Stage::DispatchSyncLast:phase_=Stage::AwaitSyncLast;break;
 case Stage::DispatchExitFirst:phase_=Stage::AwaitExitFirst;break;case Stage::DispatchExitSecond:phase_=Stage::AwaitExitSecond;break;case Stage::ReturnExit:phase_=Stage::Complete;break;default:next();break;
 }used+=cost;}return used;}
private:
 void next(){phase_=Stage(unsigned(phase_)+1);}Kind kind_;Stage phase_;uint8_t rate_=0,clock_=0,flags_=0,sync_=0;uint16_t input_=0;uint32_t output_=0;
};
}
