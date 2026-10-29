#pragma once
#include <cstdint>

namespace kstation {
// Adapted from KS SerialIrqTask/InterruptService's typed, budgeted IO contract.
// K has its own realtime-byte latch, queue resources and measured intervals.
// State is peripheral data and a continuation, never a CPU frame or code address.
class SerialTxService final {
public:
    enum class Stage {
        RetainPrimary, ReadRealtime, ChooseRealtime, ClearRealtimeValue,
        ClearRealtime, RetainRealtimeSource, SkipQueue, ReadConsumer,
        ReadProducer, CompareQueue, ChooseQueue, DisableValue, Disable,
        RestoreEmpty, FinishEmpty, RetainQueuedSource, LocateData,
        NormalizeConsumer, ResolveData, NextConsumer, MaskConsumer,
        PublishConsumer, ReadData, Transmit, ReadStatus, MaskStatus,
        WriteStatus, RestoreSource, RestorePrimary, Finish, Complete
    };
    enum class Read { Realtime, Consumer, Producer, Data, Status };
    enum class Write { Realtime, Consumer, Transmit, Control, Status };
    // Credits for non-musical retention/return intervals. The migration bridge
    // supplies dynamic bus costs; a later native owner can supply measured credits.
    struct Credits { unsigned retain=10, restore=10, finish=7, clearRealtime=1; };
    Stage stage() const { return stage_; }
    bool complete() const { return stage_==Stage::Complete; }
    uint8_t realtime() const { return realtime_; }
    uint8_t data() const { return data_; }
    uint8_t status() const { return status_; }
    uint16_t consumer() const { return consumer_; }
    uint16_t producer() const { return producer_; }
    uint16_t nextConsumer() const { return next_; }
    uint32_t dataReference() const { return 0xc496u+consumer_; }
    unsigned nextClocks(const Credits& c) const {
        switch(stage_) {
        case Stage::RetainPrimary: case Stage::RetainRealtimeSource:
        case Stage::RetainQueuedSource: return c.retain;
        case Stage::RestoreEmpty: case Stage::RestoreSource:
        case Stage::RestorePrimary: return c.restore;
        case Stage::FinishEmpty: case Stage::Finish: return c.finish;
        case Stage::ReadRealtime: return 10;
        case Stage::ClearRealtime: return 9+c.clearRealtime;
        case Stage::ReadConsumer: case Stage::ReadProducer:
        case Stage::PublishConsumer: return 12;
        case Stage::LocateData: return 9;
        case Stage::MaskConsumer: case Stage::ReadData: return 6;
        case Stage::Transmit: case Stage::ReadStatus: case Stage::WriteStatus:
        case Stage::Disable: return 4;
        case Stage::Complete: return 0;
        default: return 3;
        }
    }
    // Admit exactly one complete observation interval. An insufficient budget
    // performs no callback, data read, publication or continuation change.
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,const Credits& credits,Reader&& read,Writer&& write) {
        const auto cost=nextClocks(credits);
        if(!cost||cost>available)return 0;
        switch(stage_) {
        case Stage::RetainPrimary: stage_=Stage::ReadRealtime; break;
        case Stage::ReadRealtime: data_=realtime_=uint8_t(read(Read::Realtime)); stage_=Stage::ChooseRealtime; break;
        case Stage::ChooseRealtime: stage_=realtime_?Stage::ClearRealtimeValue:Stage::ReadConsumer; break;
        case Stage::ClearRealtimeValue: stage_=Stage::ClearRealtime; break;
        case Stage::ClearRealtime: write(Write::Realtime,0); stage_=Stage::RetainRealtimeSource; break;
        case Stage::RetainRealtimeSource: stage_=Stage::SkipQueue; break;
        case Stage::SkipQueue: stage_=Stage::Transmit; break;
        case Stage::ReadConsumer: consumer_=uint16_t(read(Read::Consumer)); stage_=Stage::ReadProducer; break;
        case Stage::ReadProducer: producer_=uint16_t(read(Read::Producer)); stage_=Stage::CompareQueue; break;
        case Stage::CompareQueue: stage_=Stage::ChooseQueue; break;
        case Stage::ChooseQueue: stage_=consumer_==producer_?Stage::DisableValue:Stage::RetainQueuedSource; break;
        case Stage::DisableValue: stage_=Stage::Disable; break;
        case Stage::Disable: write(Write::Control,0x70); stage_=Stage::RestoreEmpty; break;
        case Stage::RestoreEmpty: stage_=Stage::FinishEmpty; break;
        case Stage::FinishEmpty: stage_=Stage::Complete; break;
        case Stage::RetainQueuedSource: stage_=Stage::LocateData; break;
        case Stage::LocateData: stage_=Stage::NormalizeConsumer; break;
        case Stage::NormalizeConsumer: stage_=Stage::ResolveData; break;
        case Stage::ResolveData: stage_=Stage::NextConsumer; break;
        case Stage::NextConsumer: next_=uint16_t(consumer_+1); stage_=Stage::MaskConsumer; break;
        case Stage::MaskConsumer: next_&=255; stage_=Stage::PublishConsumer; break;
        case Stage::PublishConsumer: write(Write::Consumer,next_); stage_=Stage::ReadData; break;
        case Stage::ReadData: data_=uint8_t(read(Read::Data)); stage_=Stage::Transmit; break;
        case Stage::Transmit: write(Write::Transmit,data_); stage_=Stage::ReadStatus; break;
        case Stage::ReadStatus: status_=uint8_t(read(Read::Status)); stage_=Stage::MaskStatus; break;
        case Stage::MaskStatus: status_&=0x7f; stage_=Stage::WriteStatus; break;
        case Stage::WriteStatus: write(Write::Status,status_); stage_=Stage::RestoreSource; break;
        case Stage::RestoreSource: stage_=Stage::RestorePrimary; break;
        case Stage::RestorePrimary: stage_=Stage::Finish; break;
        case Stage::Finish: stage_=Stage::Complete; break;
        case Stage::Complete: break;
        }
        return cost;
    }
private:
    Stage stage_=Stage::RetainPrimary;
    uint8_t realtime_=0,data_=0,status_=0;
    uint16_t consumer_=0,producer_=0,next_=0;
};
}
