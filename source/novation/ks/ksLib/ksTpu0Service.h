#pragma once
#include "ksTimerServiceTask.h"
#include "ksKeyboardScanTask.h"
#include "ksKeyboardEventsTask.h"
#include "ksSerialTransmitTask.h"
#include "ksPanelDecayTask.h"
#include "ksPanelActionTask.h"
#include "ksPanelPollTask.h"
#include "ksPanelByteTask.h"
#include <optional>

namespace ks {
// Complete TPU0 call graph. Each child returns an explicit logical result to
// its owner. No child uses the compatibility Calls ABI or a CPU continuation.
class Tpu0Service final {
public:
    enum class Component { Timer, Scan, Contacts, KeyEvent, Velocity, Clock,
                           Midi, Decay, Latch, Poll, PanelByte };
    bool complete() const { return timer_.complete(); }
    bool critical() const { return critical_; }
    uint32_t components() const { return components_; }
    Component component() const { return active_; }
    unsigned nextClocks() const {
        switch(active_) {
        case Component::Timer:return timer_.nextClocks();
        case Component::Scan:return scan_->nextClocks(1);
        case Component::Contacts:return contacts_->nextClocks(3);
        case Component::KeyEvent:return key_->nextClocks(keyTransferClocks(*key_));
        case Component::Velocity:return velocity_->nextClocks();
        case Component::Clock:return clock_->nextClocks();
        case Component::Midi:return midi_->nextClocks([&](SerialTransmitTask::Write what){return what==SerialTransmitTask::Write::Enable?1u:byteClocks(midi_->dataAddress());});
        case Component::Decay:return decay_->nextClocks();
        case Component::Latch:return latch_->nextClocks();
        case Component::Poll:return poll_->nextClocks();
        case Component::PanelByte:return byte_->nextClocks([](PanelByteTask::Port){return 1u;});
        }
        return 0;
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write) {
        const auto cost=nextClocks();if(!cost||cost>available)return 0;
        return advancePlannedPhase(cost,read,write);
    }
private:
    friend class InterruptService;
    // The owner synchronously passes this phase's admitted nonzero cost.
    template<class Reader,class Writer>
    unsigned advancePlannedPhase(unsigned cost,Reader&& read,Writer&& write){
        // Transfers are observed at original clock offsets within the phase.
        const auto readWord=[&](uint32_t a,unsigned end){
            const auto gap=wordClocks(a)-byteClocks(a);
            const auto hi=uint8_t(read(a,end-gap));
            return uint16_t((unsigned(hi)<<8)|uint8_t(read(a+1,end)));
        };
        const auto writeWord=[&](uint32_t a,uint16_t v,unsigned end){
            const auto gap=wordClocks(a)-byteClocks(a);
            write(a,uint8_t(v>>8),end-gap);write(a+1,uint8_t(v),end);
        };
        const auto readLong=[&](uint32_t a,unsigned end){
            uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(read(a+n,end-9+3*n));return v;
        };
        const auto writeLong=[&](uint32_t a,uint32_t v,unsigned end){
            for(unsigned n=0;n<4;++n)write(a+n,uint8_t(v>>(24-8*n)),end-9+3*n);
        };
        switch(active_) {
        case Component::Timer:{
            using T=TimerInterruptTask;
            timer_.advanceBefore(cost,[&](T::Read field)->uint32_t{
                switch(field){case T::Read::Status:return read(0xffffd5,cost);
                case T::Read::BoardMode:return read(0xffec14,cost);
                case T::Read::Divider:return read(0xffee70,cost);
                case T::Read::Counter:return readLong(0xffec10,cost);}return 0;
            },[&](T::Write field,uint32_t v){
                if(field==T::Write::Counter)writeLong(0xffec10,v,cost);
                else write(field==T::Write::Acknowledge?0xffffd5:0xffee70,uint8_t(v),cost);
            });
            if(timer_.awaitingChild())switch(timer_.child()){
            case T::Child::BoardScan:scan_.emplace(KeyboardScanTask::Kind::Scan);select(Component::Scan);break;
            case T::Child::Clock:clock_.emplace();select(Component::Clock);break;
            case T::Child::PanelDecay:decay_.emplace();select(Component::Decay);break;
            }
            break;
        }
        case Component::Scan:case Component::Contacts:{
            using T=KeyboardScanTask;
            const bool scanning=active_==Component::Scan;auto&t=scanning?*scan_:*contacts_;
            t.advanceBefore(cost,[&](T::Read field,unsigned page)->uint16_t{
                switch(field){case T::Read::Tick:return readWord(0xffee76,cost);
                case T::Read::Upper:return read(0xffff55,cost);
                case T::Read::UpperAux:return read(0xffff54,cost);
                case T::Read::Lower:return read(0xffff59,cost);
                case T::Read::LowerAux:return read(0xffff50,cost);
                case T::Read::Page:return readWord(0xb960+2*page,cost-2);}return 0;
            },[&](T::Write field,unsigned,uint16_t v){
                if(field==T::Write::Tick)writeWord(0xffee76,v,cost);
                else if(field==T::Write::Mux)write(0xffff6d,uint8_t(v),cost);
                else writeWord(t.pageRef(),v,cost-2);
            },scanning?1:3);
            if(t.awaitingChild()){
                if(scanning){const auto page=t.pageIndex();contacts_.emplace(T::Kind::Transitions,0xb960+page*2,0xb970+page*32,t.current(),t.previous());select(Component::Contacts);}
                else {key_.emplace(static_cast<KeyboardEventsTask::Kind>(t.transition()),t.record());select(Component::KeyEvent);}
            }else if(t.complete()){
                if(scanning){scan_.reset();timer_.resumeChild();select(Component::Timer);}
                else {contacts_.reset();scan_->resumeChild();select(Component::Scan);}
            }
            break;
        }
        case Component::KeyEvent:case Component::Velocity:{
            using T=KeyboardEventsTask;using S=T::Stage;
            const bool calculating=active_==Component::Velocity;auto&t=calculating?*velocity_:*key_;
            const auto stage=t.stage();
            t.advanceBefore(cost,[&](T::Read field)->uint16_t{
                switch(field){case T::Read::Tick:return readWord(0xffee76,cost);
                case T::Read::Start:return readWord(t.record()+2,cost);
                case T::Read::Flags:return read(t.record(),stage==S::SetPressed||stage==S::ClearPressed?cost-3:cost);
                case T::Read::Key:return read(t.record()+1,cost);
                case T::Read::Producer:return readWord(0x156e8,cost);
                case T::Read::Consumer:return readWord(0x156ea,cost);
                case T::Read::Event:return readWord(0x156ec+t.consumer(),cost);}return 0;
            },[&](T::Write field,uint16_t v){
                if(field==T::Write::Flags)write(t.record(),uint8_t(v),cost);
                else writeWord(field==T::Write::Start?t.record()+2:field==T::Write::Producer?0x156e8:field==T::Write::Consumer?0x156ea:0x156ec+t.producer(),v,cost);
            },calculating?3:keyTransferClocks(t));
            if(t.awaitingChild()){velocity_.emplace(T::Kind::Velocity,t.record(),t.delay());select(Component::Velocity);}
            else if(t.complete()){
                if(calculating){const auto result=t.velocity();velocity_.reset();key_->resumeChild(result);select(Component::KeyEvent);}
                else {key_.reset();contacts_->resumeChild();select(Component::Contacts);}
            }
            break;
        }
        case Component::Clock:{
            using T=ClockAccumulatorTask;
            clock_->advanceBefore(cost,[&](T::Read field)->uint32_t{
                switch(field){case T::Read::Increment:return readLong(0xffee7e,cost);
                case T::Read::Phase:return readLong(0xffee7a,cost);
                case T::Read::PulseCount:return readWord(0xffee78,cost);
                case T::Read::OutputFlags:case T::Read::RunFlags:return read(0xffee97,cost);
                case T::Read::Secondary:return readLong(0xffee82,cost);
                case T::Read::SecondaryIncrement:return readLong(0xffee86,cost);
                case T::Read::Timeout:return read(0xffee9a,cost);}return 0;
            },[&](T::Write field,uint32_t v){
                switch(field){case T::Write::Phase:writeLong(0xffee7a,v,cost);break;
                case T::Write::PulseCount:writeWord(0xffee78,uint16_t(v),cost);break;
                case T::Write::Secondary:writeLong(0xffee82,v,cost);break;
                case T::Write::Timeout:write(0xffee9a,uint8_t(v),cost);break;
                case T::Write::RunFlags:write(0xffee97,uint8_t(v),cost);break;}
            });
            if(clock_->awaitingOutput()){midi_.emplace();select(Component::Midi);}
            else if(clock_->complete()){clock_.reset();timer_.resumeChild();select(Component::Timer);}
            break;
        }
        case Component::Midi:{
            using T=SerialTransmitTask;using S=T::Stage;const auto stage=midi_->stage();
            midi_->advanceBefore(cost,[&](T::Read field)->uint16_t{
                return field==T::Read::Input?0xf8:readWord(field==T::Read::Producer?0xffed6c:0xffed6e,cost);
            },[&](T::Write field,uint16_t v){
                if(field==T::Write::Producer)writeWord(0xffed6c,v,cost);
                else write(field==T::Write::Data?midi_->dataAddress():0xffff8a,uint8_t(v),cost);
            },[&](T::Write field){return field==T::Write::Enable?1u:byteClocks(midi_->dataAddress());});
            if(stage==S::MaskCritical)critical_=true;
            if(stage==S::RestoreCritical)critical_=false;
            if(midi_->stage()==S::Complete){midi_.reset();clock_->resumeOutput();select(Component::Clock);}
            break;
        }
        case Component::Decay:{
            using T=PanelDecayTask;
            decay_->advanceBefore(cost,[&](T::Read field,unsigned bank,unsigned index){
                return read(field==T::Read::Cell?(bank?0xffec24:0xffec16)+index:field==T::Read::Pending?(bank?0xffec29:0xffec1b):0xffec23,cost);
            },[&](T::Write field,unsigned bank,unsigned index,uint8_t v){
                write(field==T::Write::Cell?(bank?0xffec24:0xffec16)+index:field==T::Write::Pending?(bank?0xffec29:0xffec1b):0xffec23,v,cost);
            });
            if(decay_->awaiting()){latch_.emplace(PanelActionTask::Kind::Command);select(Component::Latch);}
            else if(decay_->complete()){decay_.reset();timer_.resumeChild();select(Component::Timer);}
            break;
        }
        case Component::Latch:{
            using T=PanelActionTask;
            latch_->advanceBefore(cost,[](T::Read){return uint8_t(PanelDecayTask::latchRequest());});
            if(latch_->child()==T::Child::Poll){poll_.emplace();select(Component::Poll);}
            else if(latch_->child()==T::Child::CommandByte){byte_.emplace(PanelByteTask::Kind::CommandWrite);select(Component::PanelByte);}
            else if(latch_->stage()==T::Stage::Complete){latch_.reset();decay_->resumeChild();select(Component::Decay);}
            break;
        }
        case Component::Poll:{
            poll_->advanceBefore(cost);
            if(poll_->awaiting()){byte_.emplace(PanelByteTask::Kind::CommandRead);select(Component::PanelByte);}
            else if(poll_->stage()==PanelPollTask::Stage::Complete){const auto reply=poll_->reply();poll_.reset();latch_->resumeChild(reply);select(Component::Latch);}
            break;
        }
        case Component::PanelByte:{
            using T=PanelByteTask;
            byte_->advanceBefore(cost,[&](T::Read field){return field==T::Read::Input?uint8_t(PanelDecayTask::latchRequest()):uint8_t(read(0xffff51,cost));},
                [&](T::Port port,uint8_t v){write(port==T::Port::Gate?0xfffeb1:port==T::Port::Control?0xffff62:0xffff61,v,cost);},[](T::Port){return 1u;});
            if(byte_->stage()==T::Stage::Complete){
                const bool reading=byte_->reading();const auto reply=byte_->data();byte_.reset();
                if(reading){poll_->resumeChild(reply);select(Component::Poll);}
                else {latch_->resumeChild(reply);select(Component::Latch);}
            }
            break;
        }
        }
        return cost;
    }
    static unsigned byteClocks(uint32_t a){return a>=0xfffd10?1u:3u;}
    static unsigned wordClocks(uint32_t a){return a<0x400000?3u+3u*(a&1):6u;}
    static unsigned keyTransferClocks(const KeyboardEventsTask& t){
        return t.stage()==KeyboardEventsTask::Stage::WriteAppendEvent?wordClocks(0x156ec+t.producer()):3;
    }
    void select(Component component){active_=component;components_|=1u<<unsigned(component);}
    TimerInterruptTask timer_;
    std::optional<KeyboardScanTask> scan_,contacts_;
    std::optional<KeyboardEventsTask> key_,velocity_;
    std::optional<ClockAccumulatorTask> clock_;
    std::optional<SerialTransmitTask> midi_;
    std::optional<PanelDecayTask> decay_;
    std::optional<PanelActionTask> latch_;
    std::optional<PanelPollTask> poll_;
    std::optional<PanelByteTask> byte_;
    Component active_=Component::Timer;
    uint32_t components_=1;
    bool critical_=false;
};
}
