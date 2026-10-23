#pragma once
#include <cstdint>

namespace ks {
// Four ADC channels are sampled high byte then low byte. Each half-product is
// truncated separately before addition, matching retained sample smoothing.
class PanelAdcSamplesTask final {
public:
    enum class Stage { ClearGroup, ReadGroup, ScaleGroup, SelectSamples,
        SelectAdc, SetCount, ReadHigh, ReadLow, ReadPrevious, SetRawWeight,
        WeightRaw, BindRawHalf, SetPreviousWeight, WeightPrevious, AddHalves,
        SelectSaturation, Saturate, WriteSample, AdvanceSample, DecrementCount,
        MoreChannels, Return, Complete };
    enum class Read { Group, High, Low, Previous };
    Stage stage()const{return stage_;}
    bool complete()const{return stage_==Stage::Complete;}
    uint8_t group()const{return group_;}
    uint8_t channel()const{return channel_;}
    uint8_t offset()const{return uint8_t(group_<<1);}
    uint8_t high()const{return high_;}
    uint8_t low()const{return low_;}
    uint16_t previous()const{return previous_;}
    uint16_t value()const{return uint16_t(sum_);}
    unsigned nextClocks()const{
        switch(stage_){
        case Stage::ReadGroup:return 12;
        case Stage::SelectSamples:case Stage::SelectAdc:return 9;
        case Stage::ReadHigh:case Stage::ReadLow:case Stage::ReadPrevious:
        case Stage::SetRawWeight:case Stage::SetPreviousWeight:case Stage::Saturate:
        case Stage::WriteSample:return 6;
        case Stage::WeightRaw:case Stage::WeightPrevious:return 23;
        case Stage::Return:return 17;case Stage::Complete:return 0;default:return 3;
        }
    }
    template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){
        const auto cost=nextClocks();if(!cost||cost>available)return 0;
        switch(stage_){
        case Stage::ReadGroup:group_=uint8_t(read(Read::Group));next();break;
        case Stage::ReadHigh:high_=uint8_t(read(Read::High));next();break;
        case Stage::ReadLow:low_=uint8_t(read(Read::Low));next();break;
        case Stage::ReadPrevious:previous_=uint16_t(read(Read::Previous));next();break;
        case Stage::WeightRaw:rawHalf_=uint16_t(((unsigned(high_)<<8)|low_)>>1);next();break;
        case Stage::WeightPrevious:previousHalf_=uint16_t(previous_>>1);next();break;
        case Stage::AddHalves:sum_=unsigned(rawHalf_)+previousHalf_;next();break;
        case Stage::SelectSaturation:stage_=sum_>65535?Stage::Saturate:Stage::WriteSample;break;
        case Stage::Saturate:sum_=65535;next();break;
        case Stage::WriteSample:write(uint16_t(sum_));next();break;
        case Stage::AdvanceSample:++channel_;next();break;
        case Stage::MoreChannels:stage_=channel_<4?Stage::ReadHigh:Stage::Return;break;
        case Stage::Return:stage_=Stage::Complete;break;
        default:next();break;
        }return cost;
    }
private:
    void next(){stage_=Stage(unsigned(stage_)+1);}
    Stage stage_=Stage::ClearGroup;uint8_t group_=0,channel_=0,high_=0,low_=0;
    uint16_t previous_=0,rawHalf_=0,previousHalf_=0;unsigned sum_=0;
};
}
