#pragma once
#include <cstdint>

namespace kstation {
// K receive service using the KS SerialIrqTask/SerialReceiveService contract:
// typed peripheral continuation, live IO, and one fully admitted interval.
// Shared clock feedback is a typed child; this state has no CPU dependency.
class SerialRxService final {
public:
    enum class Stage {
        RetainPrimary, RetainAddress, RetainData, ReadStatus, MaskStatus,
        WriteStatus, ReadReceive, CompareClock, AdmitClock, DispatchClock,
        SkipClock, ReadProducer, NormalizeProducer, LocateData, ResolveData,
        WriteData, NextProducer, MaskProducer, ReadConsumer, CompareFull,
        AdmitFull, PublishProducer, SkipPublish, SetError, WriteError,
        RestoreData, RestoreAddress, RestorePrimary, Finish, AwaitClock, Complete
    };
    enum class Read { Status, Receive, Producer, Consumer };
    enum class Write { Status, Data, Producer, Error };
    struct Credits { unsigned retain=10, restore=10, finish=7, dispatch=10, dataWrite=1; };
    Stage stage() const { return stage_; }
    bool complete() const { return stage_==Stage::Complete; }
    bool awaitingClock() const { return stage_==Stage::AwaitClock; }
    bool resumeClock() { if(!awaitingClock())return false;stage_=Stage::SkipClock;return true; }
    uint8_t status() const { return status_; }
    uint8_t data() const { return data_; }
    uint16_t producer() const { return producer_; }
    uint16_t consumer() const { return consumer_; }
    uint16_t nextProducer() const { return next_; }
    uint32_t dataReference() const { return (0xfffd30u+producer_)&0xffffffu; }
    unsigned nextClocks(const Credits& c) const {
        switch(stage_) {
        case Stage::RetainPrimary:case Stage::RetainAddress:case Stage::RetainData:return c.retain;
        case Stage::RestoreData:case Stage::RestoreAddress:case Stage::RestorePrimary:return c.restore;
        case Stage::Finish:return c.finish;
        case Stage::DispatchClock:return c.dispatch;
        case Stage::ReadStatus:case Stage::WriteStatus:case Stage::ReadReceive:return 4;
        case Stage::ReadProducer:case Stage::ReadConsumer:case Stage::PublishProducer:case Stage::WriteError:return 10;
        case Stage::LocateData:return 9;
        case Stage::MaskProducer:return 6;
        case Stage::WriteData:return 3+c.dataWrite;
        case Stage::AwaitClock:case Stage::Complete:return 0;
        default:return 3;
        }
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,const Credits& credits,Reader&& read,Writer&& write) {
        const auto cost=nextClocks(credits);
        if(!cost||cost>available)return 0;
        switch(stage_) {
        case Stage::RetainPrimary:stage_=Stage::RetainAddress;break;
        case Stage::RetainAddress:stage_=Stage::RetainData;break;
        case Stage::RetainData:stage_=Stage::ReadStatus;break;
        case Stage::ReadStatus:status_=uint8_t(read(Read::Status));stage_=Stage::MaskStatus;break;
        case Stage::MaskStatus:status_&=0xbf;stage_=Stage::WriteStatus;break;
        case Stage::WriteStatus:write(Write::Status,status_);stage_=Stage::ReadReceive;break;
        case Stage::ReadReceive:data_=uint8_t(read(Read::Receive));stage_=Stage::CompareClock;break;
        case Stage::CompareClock:stage_=Stage::AdmitClock;break;
        case Stage::AdmitClock:stage_=data_==0xf8?Stage::DispatchClock:Stage::ReadProducer;break;
        case Stage::DispatchClock:stage_=Stage::AwaitClock;break;
        case Stage::SkipClock:stage_=Stage::RestoreData;break;
        case Stage::ReadProducer:producer_=uint16_t(read(Read::Producer));stage_=Stage::NormalizeProducer;break;
        case Stage::NormalizeProducer:stage_=Stage::LocateData;break;
        case Stage::LocateData:stage_=Stage::ResolveData;break;
        case Stage::ResolveData:stage_=Stage::WriteData;break;
        // The original writes the ring BEFORE checking whether the next index
        // equals consumer. Overflow latches an error but does not publish producer.
        case Stage::WriteData:write(Write::Data,data_);stage_=Stage::NextProducer;break;
        case Stage::NextProducer:next_=uint16_t(producer_+1);stage_=Stage::MaskProducer;break;
        case Stage::MaskProducer:next_&=255;stage_=Stage::ReadConsumer;break;
        case Stage::ReadConsumer:consumer_=uint16_t(read(Read::Consumer));stage_=Stage::CompareFull;break;
        case Stage::CompareFull:stage_=Stage::AdmitFull;break;
        case Stage::AdmitFull:stage_=consumer_==next_?Stage::SetError:Stage::PublishProducer;break;
        case Stage::PublishProducer:write(Write::Producer,next_);stage_=Stage::SkipPublish;break;
        case Stage::SkipPublish:stage_=Stage::RestoreData;break;
        case Stage::SetError:stage_=Stage::WriteError;break;
        case Stage::WriteError:write(Write::Error,2);stage_=Stage::RestoreData;break;
        case Stage::RestoreData:stage_=Stage::RestoreAddress;break;
        case Stage::RestoreAddress:stage_=Stage::RestorePrimary;break;
        case Stage::RestorePrimary:stage_=Stage::Finish;break;
        case Stage::Finish:stage_=Stage::Complete;break;
        case Stage::AwaitClock:case Stage::Complete:break;
        }
        return cost;
    }
private:
    Stage stage_=Stage::RetainPrimary;
    uint8_t status_=0,data_=0;
    uint16_t producer_=0,consumer_=0,next_=0;
};
}
