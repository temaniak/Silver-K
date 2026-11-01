#pragma once
#include <cstdint>
namespace ks {
// Three recovered integer ADC curves. Retain byte-only upper clipping and the
// five-count center plateau; neither floating-point normalization nor rounding.
class PanelAdcScaleTask final {
public:
    enum class Kind { Ordinary, Special };
    enum class Stage { SpecialWeight, SpecialMultiply, SpecialHalf, SpecialClearHigh, SpecialDouble, SpecialOffset, SpecialSelectFloor, SpecialFloor, SpecialCompareLimit, SpecialSelectLimit, SpecialLimit, SpecialPublish, ReadFlag, SelectCurve, BipolarWeight, BipolarMultiply, BipolarHalf, BipolarClearHigh, BipolarDouble, BipolarOffset, BipolarSelectFloor, BipolarFloor, BipolarCompareLimit, BipolarSelectLimit, BipolarLimit, CopyCenterValue, SetCenter, SubtractCenter, SelectCenter, CompareCenterWidth, SelectCenterWidth, LimitCenterWidth, ApplyCenterWidth, SkipLinear, LinearWeight, LinearMultiply, LinearHalf, LinearClearHigh, LinearDouble, LinearOffset, LinearSelectFloor, LinearFloor, LinearCompareLimit, LinearSelectLimit, LinearLimit, WriteLastChannel, BindChannel, Handoff, BindSpecialChannel, DispatchSpecial, Complete };
    enum class Read { CurveFlag };
    enum class Write { LastChannel };
    PanelAdcScaleTask(Kind kind,uint16_t raw,uint8_t channel):stage_(kind==Kind::Special?Stage::SpecialWeight:Stage::ReadFlag),raw_(raw),channel_(channel){}
    Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
    uint8_t curveFlag()const{return curveFlag_;}uint8_t channel()const{return channel_;}
    uint32_t value()const{return value_;}uint8_t scaled()const{return uint8_t(value_>>8);}
    unsigned nextClocks()const{
        switch(stage_){
        case Stage::SpecialWeight:case Stage::ReadFlag:case Stage::BipolarWeight:case Stage::LinearWeight:return 6;
        case Stage::SpecialPublish:return 8;
        case Stage::SpecialOffset:case Stage::SpecialCompareLimit:case Stage::BipolarOffset:case Stage::BipolarCompareLimit:case Stage::LinearOffset:case Stage::LinearCompareLimit:return 9;
        case Stage::WriteLastChannel:return 12;
        case Stage::DispatchSpecial:return 15;
        case Stage::SpecialMultiply:case Stage::BipolarMultiply:case Stage::LinearMultiply:return 23;
        case Stage::Complete:return 0;default:return 3;
        }
    }
    template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){
        const auto cost=nextClocks();if(!cost||cost>available)return 0;
        switch(stage_){
        case Stage::ReadFlag:curveFlag_=uint8_t(read(Read::CurveFlag));next();break;
        case Stage::SelectCurve:stage_=curveFlag_?Stage::BipolarWeight:Stage::LinearWeight;break;
        case Stage::SpecialMultiply:value_=(uint32_t(raw_)*0x8207)>>16;next();break;
        case Stage::BipolarMultiply:value_=(uint32_t(raw_)*0x43cc)>>16;next();break;
        case Stage::LinearMultiply:value_=(uint32_t(raw_)*0x4144)>>16;next();break;
        case Stage::SpecialDouble:case Stage::BipolarDouble:case Stage::LinearDouble:value_*=2;next();break;
        case Stage::SpecialOffset:offset_=512;subtractOffset();next();break;
        case Stage::BipolarOffset:case Stage::LinearOffset:offset_=256;subtractOffset();next();break;
        case Stage::SpecialSelectFloor:stage_=underflow_?Stage::SpecialFloor:Stage::SpecialCompareLimit;break;
        case Stage::BipolarSelectFloor:stage_=underflow_?Stage::BipolarFloor:Stage::BipolarCompareLimit;break;
        case Stage::LinearSelectFloor:stage_=underflow_?Stage::LinearFloor:Stage::LinearCompareLimit;break;
        case Stage::SpecialFloor:case Stage::BipolarFloor:case Stage::LinearFloor:value_=0;next();break;
        case Stage::SpecialSelectLimit:stage_=value_>0xff00?Stage::SpecialLimit:Stage::SpecialPublish;break;
        case Stage::BipolarSelectLimit:stage_=value_>0x8400?Stage::BipolarLimit:Stage::CopyCenterValue;break;
        case Stage::LinearSelectLimit:stage_=value_>0x7f00?Stage::LinearLimit:Stage::WriteLastChannel;break;
        case Stage::SpecialLimit:clipUpper(255);next();break;
        case Stage::BipolarLimit:clipUpper(132);next();break;
        case Stage::LinearLimit:clipUpper(127);next();break;
        case Stage::SpecialPublish:stage_=Stage::BindSpecialChannel;break;
        case Stage::SubtractCenter:excess_=uint8_t(scaled()-64);next();break;
        case Stage::SelectCenter:stage_=scaled()<=64?Stage::SkipLinear:Stage::CompareCenterWidth;break;
        case Stage::SelectCenterWidth:stage_=excess_<=5?Stage::ApplyCenterWidth:Stage::LimitCenterWidth;break;
        case Stage::LimitCenterWidth:excess_=5;next();break;
        case Stage::ApplyCenterWidth:clipUpper(uint8_t(scaled()-excess_));next();break;
        case Stage::SkipLinear:stage_=Stage::WriteLastChannel;break;
        case Stage::WriteLastChannel:write(Write::LastChannel,channel_);next();break;
        case Stage::Handoff:case Stage::DispatchSpecial:stage_=Stage::Complete;break;
        default:next();break;
        }return cost;
    }
private:
    void next(){stage_=Stage(unsigned(stage_)+1);}void subtractOffset(){underflow_=value_<offset_;value_-=offset_;}
    void clipUpper(uint8_t high){value_=(value_&0xffff00ffu)|(uint32_t(high)<<8);}
    Stage stage_;uint16_t raw_;uint8_t channel_,curveFlag_=0,excess_=0;uint32_t value_=0,offset_=0;bool underflow_=false;
};
}
