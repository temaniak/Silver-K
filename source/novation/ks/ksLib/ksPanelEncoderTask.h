#pragma once
#include <cstdint>

namespace ks {
// Encoder timer differences use wrapping bytes, including the original
// acceleration decay and reset on a sign change. No absolute-time approximation.
class PanelEncoderTask final {
public:
    enum class Stage {
        ReadTimer, HalfTimer, ReadHalfHistory, WriteHalfHistory, HalfDifference,
        SelectHalfSign, AbsHalfDifference, ReadActivity, AddActivity, SelectZeroActivity,
        DecayActivity, CompareActivityLimit, SelectActivityLimit, ClipActivity,
        WriteActivity, SetFloor, SubtractFloor, SelectFloor, ClearFloor, IncrementGain,
        QuarterTimer, RetainQuarter, ReadQuarterHistory, WriteQuarterHistory,
        QuarterDifference, SelectStationary, ReadDeltaHistory, WriteDeltaHistory,
        CompareDirection, SelectDirectionChange, ClearActivity, WriteClearedActivity,
        ResetGain, TestDirection, SelectDirectionSign, NegateGain, BindResult,
        Return, Complete
    };
    enum class Read { Timer, HalfHistory, Activity, QuarterHistory, DeltaHistory };
    enum class Write { HalfHistory, Activity, QuarterHistory, DeltaHistory };
    Stage stage()const{return stage_;}
    bool complete()const{return stage_==Stage::Complete;}
    uint16_t timer()const{return timer_;}
    uint8_t previousHalf()const{return previousHalf_;}
    uint8_t activity()const{return activity_;}
    uint8_t previousQuarter()const{return previousQuarter_;}
    uint8_t previousDelta()const{return previousDelta_;}
    uint8_t result()const{return result_;}
    unsigned nextClocks()const{
        switch(stage_){
        case Stage::ReadTimer:return 8;
        case Stage::ReadHalfHistory:case Stage::WriteHalfHistory:case Stage::ReadActivity:
        case Stage::WriteActivity:case Stage::ReadQuarterHistory:case Stage::WriteQuarterHistory:
        case Stage::ReadDeltaHistory:case Stage::WriteDeltaHistory:case Stage::WriteClearedActivity:return 12;
        case Stage::Return:return 17;case Stage::Complete:return 0;default:return 3;
        }
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){
        const auto cost=nextClocks();if(!cost||cost>available)return 0;
        switch(stage_){
        case Stage::ReadTimer:timer_=uint16_t(read(Read::Timer));next();break;
        case Stage::ReadHalfHistory:previousHalf_=uint8_t(read(Read::HalfHistory));next();break;
        case Stage::WriteHalfHistory:write(Write::HalfHistory,uint8_t(timer_>>1));next();break;
        case Stage::HalfDifference:halfDifference_=uint8_t(previousHalf_-uint8_t(timer_>>1));next();break;
        case Stage::SelectHalfSign:stage_=halfDifference_&128?Stage::AbsHalfDifference:Stage::ReadActivity;break;
        case Stage::AbsHalfDifference:halfDifference_=uint8_t(0-halfDifference_);next();break;
        case Stage::ReadActivity:activity_=uint8_t(read(Read::Activity));next();break;
        case Stage::AddActivity:activity_=uint8_t(activity_+halfDifference_);next();break;
        case Stage::SelectZeroActivity:stage_=activity_?Stage::DecayActivity:Stage::WriteActivity;break;
        case Stage::DecayActivity:--activity_;next();break;
        case Stage::SelectActivityLimit:stage_=activity_<=13?Stage::WriteActivity:Stage::ClipActivity;break;
        case Stage::ClipActivity:activity_=13;next();break;
        case Stage::WriteActivity:write(Write::Activity,activity_);next();break;
        case Stage::SubtractFloor:gain_=uint8_t(activity_-4);next();break;
        case Stage::SelectFloor:stage_=gain_&128?Stage::ClearFloor:Stage::IncrementGain;break;
        case Stage::ClearFloor:gain_=0;next();break;
        case Stage::IncrementGain:++gain_;next();break;
        case Stage::ReadQuarterHistory:previousQuarter_=uint8_t(read(Read::QuarterHistory));next();break;
        case Stage::WriteQuarterHistory:write(Write::QuarterHistory,uint8_t(timer_>>2));next();break;
        case Stage::QuarterDifference:delta_=uint8_t(previousQuarter_-uint8_t(timer_>>2));next();break;
        case Stage::SelectStationary:stage_=delta_?Stage::ReadDeltaHistory:Stage::Return;break;
        case Stage::ReadDeltaHistory:previousDelta_=uint8_t(read(Read::DeltaHistory));next();break;
        case Stage::WriteDeltaHistory:write(Write::DeltaHistory,delta_);next();break;
        case Stage::CompareDirection:directionChanged_=bool((previousDelta_^delta_)&128);next();break;
        case Stage::SelectDirectionChange:stage_=directionChanged_?Stage::ClearActivity:Stage::TestDirection;break;
        case Stage::ClearActivity:gain_=activity_=0;next();break;
        case Stage::WriteClearedActivity:write(Write::Activity,0);next();break;
        case Stage::ResetGain:gain_=1;next();break;
        case Stage::SelectDirectionSign:stage_=delta_&128?Stage::BindResult:Stage::NegateGain;break;
        case Stage::NegateGain:gain_=uint8_t(0-gain_);next();break;
        case Stage::BindResult:result_=gain_;next();break;
        case Stage::Return:stage_=Stage::Complete;break;
        default:next();break;
        }return cost;
    }
private:
    void next(){stage_=Stage(unsigned(stage_)+1);}
    Stage stage_=Stage::ReadTimer;uint16_t timer_=0;
    uint8_t previousHalf_=0,halfDifference_=0,activity_=0,gain_=0,previousQuarter_=0,delta_=0,previousDelta_=0,result_=0;
    bool directionChanged_=false;
};
}
