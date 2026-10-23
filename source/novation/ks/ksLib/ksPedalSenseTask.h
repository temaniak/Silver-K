#pragma once
#include <cstdint>

namespace ks {
// Panel switch bits and active-low pedal transitions, with live polarity and
// history reads at their original observation points. 128 means no transition.
class PedalSenseTask final {
public:
    enum class Kind { Switches, Reset, Pedal };
    enum class Stage { ReadSwitches, MaskSwitches, ReturnSwitches,
        SetReset, WriteReset, ReturnReset,
        SaveScratch, SelectGlobals, ReadPolarity, ReadPort, RotatePolarity,
        NormalizePort, ReadHistory, WriteHistory, DetectTransition, SetUnchanged,
        TestTransition, SelectTransition, SetReleased, TestReleased, SelectReleased,
        SetPressed, RestoreScratch, ReturnPedal, Complete };
    enum class Read { Switches, Polarity, Port, History };
    explicit PedalSenseTask(Kind kind):stage_(kind==Kind::Switches?Stage::ReadSwitches:kind==Kind::Reset?Stage::SetReset:Stage::SaveScratch){}
    Stage stage()const{return stage_;}
    bool complete()const{return stage_==Stage::Complete;}
    uint8_t switches()const{return switches_;}
    uint8_t polarity()const{return polarity_;}
    uint8_t port()const{return port_;}
    uint8_t history()const{return history_;}
    uint8_t normalized()const{return normalized_;}
    uint8_t result()const{return result_;}
    unsigned nextClocks()const{
        switch(stage_){
        case Stage::ReadSwitches:case Stage::ReadPort:return 4;
        case Stage::WriteReset:case Stage::ReadHistory:case Stage::WriteHistory:return 12;
        case Stage::SaveScratch:case Stage::RestoreScratch:return 20;
        case Stage::SelectGlobals:return 9;case Stage::ReadPolarity:return 15;
        case Stage::ReturnSwitches:case Stage::ReturnReset:case Stage::ReturnPedal:return 17;
        case Stage::Complete:return 0;default:return 3;
        }
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){
        const auto cost=nextClocks();if(!cost||cost>available)return 0;
        switch(stage_){
        case Stage::ReadSwitches:switches_=uint8_t(read(Read::Switches));next();break;
        case Stage::MaskSwitches:switches_&=0x30;next();break;
        case Stage::WriteReset:write(255);next();break;
        case Stage::ReadPolarity:polarity_=uint8_t(read(Read::Polarity));next();break;
        case Stage::ReadPort:port_=uint8_t(read(Read::Port));next();break;
        case Stage::RotatePolarity:polarity_=uint8_t((polarity_>>2)|(polarity_<<6));next();break;
        case Stage::NormalizePort:normalized_=port_^polarity_;next();break;
        case Stage::ReadHistory:history_=uint8_t(read(Read::History));next();break;
        case Stage::WriteHistory:write(normalized_);next();break;
        case Stage::DetectTransition:changed_=bool((history_^normalized_)&64);next();break;
        case Stage::SetUnchanged:result_=128;next();break;
        case Stage::SelectTransition:stage_=changed_?Stage::SetReleased:Stage::RestoreScratch;break;
        case Stage::SetReleased:result_=0;next();break;
        case Stage::SelectReleased:stage_=normalized_&64?Stage::RestoreScratch:Stage::SetPressed;break;
        case Stage::SetPressed:result_=127;next();break;
        case Stage::ReturnSwitches:case Stage::ReturnReset:case Stage::ReturnPedal:stage_=Stage::Complete;break;
        default:next();break;
        }return cost;
    }
private:
    void next(){stage_=Stage(unsigned(stage_)+1);}
    Stage stage_;uint8_t switches_=0,polarity_=0,port_=0,history_=0,normalized_=0,result_=128;bool changed_=false;
};
}
