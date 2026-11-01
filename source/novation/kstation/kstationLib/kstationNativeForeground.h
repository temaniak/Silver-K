#pragma once
#include "kstationNativeMainLoop.h"
#include "kstationNativeMidiPump.h"
#include "kstationNativeAdc.h"
#include <array>
#include <stdexcept>

namespace kstation {
// One owner for the lifted foreground graph. Remaining musical children are
// explicit waits in the semantic tasks. They are still H8 in development mode;
// this adapter must disappear before the complete native mode is admitted.
class NativeForeground final {
    enum class Kind{Main,Deferred,Pump,Dequeue,Parser,Descriptor,Utility,Tick,AdcMux};
    struct Job {
        Kind kind=Kind::Main;NativeMainLoop main;NativeDeferredSlot deferred{0};
        NativeMidiPump pump;NativeMidiDequeue dequeue{MidiDequeueTask::Kind::External};
        NativeMidiParser parser{MidiParserTask::Kind::External,0};NativeMidiDescriptor descriptor{0};
        NativeMidiUtility utility{MidiUtilityTask::Kind::ResetExternal};NativeAdcMux mux;unsigned tick=0;
    };
public:
    struct CodeRange{uint32_t begin,end;};
    inline static constexpr std::array<CodeRange,14> CodeRanges{{
        {0x65a,0x670},
        {0x9aa,0x9b6},{0x9de,0xa50},{0x20ba,0x20ce},{0x252c,0x255a},
        {0x279c,0x27f4},{0x2838,0x2904},{0x2954,0x2a82},
        {0x38b0,0x38e8},{0x390c,0x3944},{0x3968,0x39a0},
        {0x39c4,0x39fc},{0x3a20,0x3a58},{0x3ae8,0x3b20}}};
    struct Result{bool stepped=false,midi=false,main=false,tick=false;};
    void reset(){depth_=0;}
    bool active()const{return depth_!=0;}
    static bool candidate(uint32_t at){for(auto r:CodeRanges)if(at>=r.begin&&at<r.end)return true;return false;}
    Result step(Stage1Cpu&c){
        if(midi_projection::blocked(c))return {};
        const auto at=uint32_t(c.getPC());
        if(depth_){auto&j=jobs_[depth_-1];switch(j.kind){
            case Kind::Main:j.main.resumeChild(c);break;case Kind::Deferred:j.deferred.resumeChild(c);break;
            case Kind::Pump:j.pump.resumeChild(c);break;case Kind::Parser:j.parser.resumeChild(c);break;default:break;}}
        if(!depth_||boundary(jobs_[depth_-1])!=at){
            Job j;
            if(at==0x3ae8)j.kind=Kind::Main;
            else if(at>=0x38b0&&at<=0x3a20&&(at-0x38b0)%0x5c==0){j.kind=Kind::Deferred;j.deferred=NativeDeferredSlot((at-0x38b0)/0x5c);}
            else if(at==0x252c)j.kind=Kind::Pump;
            else if(at==0xa02||at==0x27b6){j.kind=Kind::Dequeue;j.dequeue=NativeMidiDequeue(at==0xa02?MidiDequeueTask::Kind::External:MidiDequeueTask::Kind::Internal);}
            else if(at==0x2954||at==0x2838){j.kind=Kind::Parser;j.parser=NativeMidiParser(at==0x2954?MidiParserTask::Kind::External:MidiParserTask::Kind::Internal,uint8_t(c.reg8(14)));}
            else if(at==0x2a56){j.kind=Kind::Descriptor;j.descriptor=NativeMidiDescriptor(uint8_t(c.reg8(8)));}
            else if(at==0x9de||at==0x279c||at==0x20ba){j.kind=Kind::Utility;j.utility=NativeMidiUtility(at==0x9de?MidiUtilityTask::Kind::ResetExternal:at==0x279c?MidiUtilityTask::Kind::ResetInternal:MidiUtilityTask::Kind::Activity);}
            else if(at==0x9aa)j.kind=Kind::Tick;
            else if(at==0x65a)j.kind=Kind::AdcMux;
            else return {};
            if(depth_==jobs_.size())throw std::logic_error("Native K foreground continuation capacity");jobs_[depth_++]=j;
        }
        auto&j=jobs_[depth_-1];bool stepped=false,done=false;const auto kind=j.kind;
        switch(kind){
        case Kind::Main:stepped=j.main.step(c);break;
        case Kind::Deferred:stepped=j.deferred.step(c);done=j.deferred.task().complete();break;
        case Kind::Pump:stepped=j.pump.step(c);done=j.pump.task().complete();break;
        case Kind::Dequeue:stepped=j.dequeue.step(c);done=j.dequeue.task().complete();break;
        case Kind::Parser:stepped=j.parser.step(c);done=j.parser.task().complete();break;
        case Kind::Descriptor:stepped=j.descriptor.step(c);done=j.descriptor.task().complete();break;
        case Kind::Utility:stepped=j.utility.step(c);done=j.utility.task().complete();break;
        case Kind::AdcMux:stepped=j.mux.step(c);done=j.mux.complete();break;
        case Kind::Tick:
            if(c.pending_irqs)c.read8(0xfffff2);c.fetchClocks(j.tick==0?6:j.tick==1?4:2);c.pc=c.makepc(int(at+1));
            if(j.tick==0){c.reg32(6)=0xfffd20;midi_projection::val(c,c.reg32(6));c.pc=c.makepc(0x9b0);}
            else if(j.tick==1){c.reg32(6)=int32_t(c.read32(0xfffd20));midi_projection::val(c,c.reg32(6));c.pc=c.makepc(0x9b4);}
            else{midi_projection::ret(c);done=true;}++j.tick;stepped=true;break;
        }
        if(done)--depth_;
        return {stepped,kind!=Kind::Main&&kind!=Kind::Deferred&&kind!=Kind::Tick&&kind!=Kind::AdcMux,kind==Kind::Main||kind==Kind::Deferred,kind==Kind::Tick};
    }
private:
    static uint32_t boundary(const Job&j){switch(j.kind){
        case Kind::Main:return j.main.task().awaitingChild()?0:j.main.boundary();
        case Kind::Deferred:return j.deferred.task().awaitingChild()?0:j.deferred.boundary();
        case Kind::Pump:return j.pump.task().awaitingChild()?0:j.pump.boundary();
        case Kind::Dequeue:return j.dequeue.boundary();
        case Kind::Parser:return j.parser.task().awaitingChild()?0:j.parser.boundary();
        case Kind::Descriptor:return j.descriptor.boundary();case Kind::Utility:return j.utility.boundary();
        case Kind::AdcMux:return j.mux.boundary();
        case Kind::Tick:return j.tick==0?0x9aa:j.tick==1?0x9b0:0x9b4;}return 0;}
    std::array<Job,16>jobs_{};unsigned depth_=0;
};
}
