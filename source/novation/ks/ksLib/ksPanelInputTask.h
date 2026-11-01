#pragma once
#include <cstdint>

namespace ks {
// Two independent firmware event rings. Reads remain live until their scheduled
// stage; the consumer advances before observing its event, as on the hardware.
class PanelInputTask final {
public:
    enum class Kind { ButtonReset, ButtonPop, ButtonPush, ControlReset, ControlPop, ControlPeek, ControlPush };
    enum class Stage {
        SaveReset, ZeroReset, ResetProducer, ResetConsumer, RestoreReset, ReturnReset,
        SavePop, ReadConsumer, ReadProducer, CompareIndices, SelectEmpty, EmptyResult,
        RestoreEmpty, ReturnEmpty, SavePopAux, SelectPopBuffer, NormalizePop, LocatePop,
        AdvanceConsumer, WrapConsumer, PublishConsumer, ReadEvent, SetAvailable,
        ConvertControl, RestorePopAux, RestorePop, ReturnPop,
        SavePushAux, SavePush, ZeroKey, CaptureKey, ReadMapping, SelectMapping,
        ReadPushProducer, NormalizePush, SelectPushBuffer, LocatePush, WriteEvent,
        AdvanceProducer, WrapProducer, PublishProducer, RestorePush, RestorePushAux,
        ReturnPush, AwaitControl, Complete
    };
    enum class Read { Consumer, Producer, Mapping, Event };
    enum class Write { Consumer, Producer, Event };
    explicit PanelInputTask(Kind kind, uint16_t event=0):kind_(kind),event_(event) {
        stage_=reset()?Stage::SaveReset:pop()?Stage::SavePop:controls()?Stage::SavePushAux:Stage::ZeroKey;
    }
    Kind kind()const{return kind_;}
    Stage stage()const{return stage_;}
    bool controls()const{return kind_==Kind::ControlReset||kind_==Kind::ControlPop||kind_==Kind::ControlPeek||kind_==Kind::ControlPush;}
    bool reset()const{return kind_==Kind::ButtonReset||kind_==Kind::ControlReset;}
    bool pop()const{return kind_==Kind::ButtonPop||kind_==Kind::ControlPop||kind_==Kind::ControlPeek;}
    bool complete()const{return stage_==Stage::Complete;}
    bool awaitingControl()const{return stage_==Stage::AwaitControl;}
    bool resumeControl(){if(!awaitingControl())return false;stage_=Stage::RestorePopAux;return true;}
    uint16_t consumer()const{return consumer_;}
    uint16_t producer()const{return producer_;}
    uint16_t event()const{return event_;}
    uint16_t eventIndex()const{return eventIndex_;}
    uint8_t key()const{return key_;}
    uint8_t mapping()const{return mapping_;}
    uint16_t mask()const{return controls()?0x1fe:0x7e;}
    unsigned nextClocks(unsigned eventClocks=3)const{
        switch(stage_){
        case Stage::SaveReset:case Stage::RestoreReset:return 11;
        case Stage::ResetProducer:case Stage::ResetConsumer:case Stage::ReadConsumer:
        case Stage::ReadProducer:case Stage::PublishConsumer:case Stage::ReadPushProducer:
        case Stage::PublishProducer:return 12;
        case Stage::SavePop:case Stage::RestoreEmpty:case Stage::SavePopAux:
        case Stage::RestorePopAux:case Stage::RestorePop:case Stage::SavePushAux:
        case Stage::SavePush:case Stage::RestorePush:case Stage::RestorePushAux:return 20;
        case Stage::EmptyResult:return controls()?3:6;
        case Stage::SelectPopBuffer:case Stage::SelectPushBuffer:return 9;
        case Stage::ReadEvent:case Stage::WriteEvent:return 3+eventClocks;
        case Stage::WrapConsumer:case Stage::SetAvailable:case Stage::WrapProducer:return 6;
        case Stage::ReadMapping:return 15;
        case Stage::ConvertControl:return 20;
        case Stage::ReturnReset:case Stage::ReturnEmpty:case Stage::ReturnPop:case Stage::ReturnPush:return 17;
        case Stage::AwaitControl:case Stage::Complete:return 0;
        default:return 3;
        }
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned eventClocks=3){
        const auto cost=nextClocks(eventClocks);if(!cost||available<cost)return 0;
        switch(stage_){
        case Stage::ResetProducer:write(Write::Producer,0);next();break;
        case Stage::ResetConsumer:write(Write::Consumer,0);next();break;
        case Stage::ReadConsumer:consumer_=uint16_t(read(Read::Consumer));next();break;
        case Stage::ReadProducer:producer_=uint16_t(read(Read::Producer));next();break;
        case Stage::SelectEmpty:stage_=consumer_==producer_?Stage::EmptyResult:Stage::SavePopAux;break;
        case Stage::LocatePop:eventIndex_=consumer_;next();break;
        case Stage::AdvanceConsumer:consumer_=uint16_t(consumer_+2);next();break;
        case Stage::WrapConsumer:consumer_&=mask();next();break;
        case Stage::PublishConsumer:write(Write::Consumer,consumer_);next();break;
        case Stage::ReadEvent:event_=uint16_t(read(Read::Event));stage_=controls()?Stage::SetAvailable:Stage::RestorePopAux;break;
        case Stage::SetAvailable:stage_=kind_==Kind::ControlPop?Stage::ConvertControl:Stage::RestorePopAux;break;
        case Stage::ConvertControl:stage_=Stage::AwaitControl;break;
        case Stage::CaptureKey:key_=uint8_t(event_);next();break;
        case Stage::ReadMapping:mapping_=uint8_t(read(Read::Mapping));event_=uint16_t((event_&0xff00)|mapping_);stage_=controls()?Stage::SelectMapping:Stage::ReadPushProducer;break;
        case Stage::SelectMapping:stage_=mapping_&128?Stage::RestorePush:Stage::ReadPushProducer;break;
        case Stage::ReadPushProducer:producer_=uint16_t(read(Read::Producer));next();break;
        case Stage::LocatePush:eventIndex_=producer_;next();break;
        case Stage::WriteEvent:write(Write::Event,event_);next();break;
        case Stage::AdvanceProducer:producer_=uint16_t(producer_+2);next();break;
        case Stage::WrapProducer:producer_&=mask();next();break;
        case Stage::PublishProducer:write(Write::Producer,producer_);stage_=controls()?Stage::RestorePush:Stage::ReturnPush;break;
        case Stage::ReturnReset:case Stage::ReturnEmpty:case Stage::ReturnPop:case Stage::ReturnPush:stage_=Stage::Complete;break;
        default:next();break;
        }
        return cost;
    }
private:
    void next(){stage_=Stage(unsigned(stage_)+1);}
    Kind kind_;Stage stage_;uint16_t event_,consumer_=0,producer_=0,eventIndex_=0;uint8_t key_=0,mapping_=0;
};
}
