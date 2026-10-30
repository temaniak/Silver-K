#pragma once
#include "kstationControllerDeviceState.h"
#include "kstationControllerInterruptRuntime.h"
#include "kstationDspTransferTask.h"
#include <array>

namespace kstation {
// Live HDI protocol binding. Domain arguments and fixed continuations replace
// the temporary H8 caller projection; no processor or instruction stream exists.
class ControllerDspTransferRuntime final {
    using W=DspWordTask;using A=DspAddressTask;using F=DspFaultTask;
    enum class Kind:uint8_t {Word,Address,Fault,Invalidate,SetAddress};
    struct Job {Kind kind=Kind::Word;W word{0};A address{0};F fault{true};unsigned setup=0;};
public:
    enum class Block:uint8_t {None,NotBound,Inactive,NeedsInterrupt,UnsupportedMode};
    struct Result {unsigned clocks=0;bool returned=false;Block blocked=Block::None;};
    bool bindVerifiedFirmware(ControllerDeviceState&d,const uint8_t*image,size_t size){
        if(bound_||active()||!image||size!=49728||!d.hasImmutableCapacity(ranges_.size()*2))return false;
        for(auto r:ranges_)for(auto a=r.begin;a<r.end;++a)
            if(d.readResource(a)!=image[a]||d.readResource(0x400000+a)!=image[a])return false;
        for(auto r:ranges_)if(!d.protectImmutableVerified(r.begin,r.end,image+r.begin)
            ||!d.protectImmutableVerified(0x400000+r.begin,0x400000+r.end,image+r.begin))return false;
        bound_=&d;return true;
    }
    bool boundTo(const ControllerDeviceState&d)const{return bound_==&d;}
    bool active()const{return depth_!=0;}void reset(){depth_=0;value_=0;}
    uint32_t receivedWord()const{return value_;}
    bool startWrite(uint16_t address,uint32_t word){if(!begin(Kind::Word))return false;jobs_[0].word=W(word,address,true);return true;}
    bool startNext(uint32_t word){if(!begin(Kind::Word))return false;jobs_[0].word=W(word);return true;}
    bool startAddress(uint16_t address){if(!begin(Kind::Address))return false;jobs_[0].address=A(address);return true;}
    bool startRead(uint16_t address){if(!begin(Kind::Address))return false;jobs_[0].address=A(address,true);return true;}
    bool startInvalidate(){return begin(Kind::Invalidate);}
    bool startSetAddress(uint16_t address){if(!begin(Kind::SetAddress))return false;jobs_[0].address=A(address);return true;}
    bool startFault(bool wrapper=true){if(!begin(Kind::Fault))return false;jobs_[0].fault=F(wrapper);return true;}
    unsigned nextClocks(const ControllerDeviceState&d,const ControllerInterruptRuntime&irq)const{
        if(bound_!=&d||!irq.boundTo(d)||!active()||d.interruptMode2||d.pendingIrqsHigh||irq.active()
            ||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return 0;
        return cost(jobs_[depth_-1])+(d.pendingIrqs?1u:0u);
    }
    Result advanceBefore(ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned available){
        if(bound_!=&d||!irq.boundTo(d))return {0,false,Block::NotBound};
        if(!active())return {0,false,Block::Inactive};
        if(d.interruptMode2||d.pendingIrqsHigh)return {0,false,Block::UnsupportedMode};
        if(irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return {0,false,Block::NeedsInterrupt};
        auto&j=jobs_[depth_-1];const auto clocks=cost(j),arb=d.pendingIrqs?1u:0u;
        if(!clocks||available<clocks+arb)return {};
        if(arb)(void)d.read8(0xfffff2);
        const auto beginClock=d.cycles;bool stackWrite=false,done=false,pushAddress=false,pushFault=false,pushLatch=false;
        auto read=[&](uint32_t at,unsigned width,unsigned end)->uint32_t{
            uint32_t v=0;d.cycles=beginClock+end;
            for(unsigned i=0;i<width;++i){d.lastRead=history(at+i);v=(v<<8)|d.readResource(at+i);}return v;
        };
        auto write=[&](uint32_t at,uint32_t v,unsigned width,unsigned end){
            d.cycles=beginClock+end;
            for(unsigned i=0;i<width;++i){d.lastWrite=history(at+i);d.writeResource(at+i,uint8_t(v>>(8*(width-i-1))));}
        };
        constexpr DspTransferCredits cr{};
        if(j.kind==Kind::Word){
            const auto stage=j.word.stage();pushAddress=stage==W::Stage::DispatchAddress;
            pushFault=stage==W::Stage::PollReady&&j.word.poll().stage()==DspPollTask::Stage::DispatchFault;
            stackWrite=pushAddress||pushFault;
            auto wordRead=[&](bool cache){return read(cache?0xc598:0x600004,cache?2:1,clocks);};
            auto wordWrite=[&](W::Write field,uint16_t v){write(field==W::Write::Cache?0xc598:field==W::Write::High?0x60000a:field==W::Write::Middle?0x60000c:0x60000e,v,field==W::Write::Cache?2:1,clocks);};
            if((arb?j.word.advanceBefore(clocks,cr,wordRead,wordWrite):j.word.advancePrepared(clocks,cr,wordRead,wordWrite))!=clocks)
                throw std::logic_error("K domain DSP word phase");
            done=j.word.complete();
        }else if(j.kind==Kind::Address){
            pushFault=j.address.polling()&&j.address.poll().stage()==DspPollTask::Stage::DispatchFault;
            stackWrite=pushFault;
            auto addressRead=[&](unsigned port){return read(0x600000+port,1,clocks);};
            auto addressWrite=[&](A::Write field,uint16_t v){write(field==A::Write::Cache?0xc598:field==A::Write::Control?0x600000:field==A::Write::High?0x60000a:field==A::Write::Middle?0x60000c:0x60000e,v,field==A::Write::Cache?2:1,clocks);};
            if((arb?j.address.advanceBefore(clocks,cr,addressRead,addressWrite):j.address.advancePrepared(clocks,cr,addressRead,addressWrite))!=clocks)
                throw std::logic_error("K domain DSP address phase");
            done=j.address.complete();if(done&&j.address.receives())value_=j.address.result();
        }else if(j.kind==Kind::Fault){
            auto s=j.fault.stage();pushLatch=s==F::Stage::DispatchLatch;
            stackWrite=pushLatch||s==F::Stage::RetainValue;
            if(j.fault.advanceBefore(clocks,cr,[&]{return read(0xffffd2,1,clocks);},[&](uint8_t v){write(0xffffd2,v,1,clocks);})!=clocks)
                throw std::logic_error("K domain DSP fault phase");
            done=j.fault.complete();
        }else if(j.kind==Kind::Invalidate){
            if(j.setup==2)write(0xc598,0xffff,2,clocks);
            done=++j.setup==4;
        }else{
            if(j.setup==0)write(0xc598,j.address.address(),2,clocks);
            else if(j.setup==1){pushAddress=true;stackWrite=true;}
            else done=true;
            ++j.setup;
        }
        d.cycles=beginClock+clocks;d.lastRead=-1;if(stackWrite)d.lastWrite=-1;
        if(pushAddress){Job child;child.kind=Kind::Address;child.address=A(j.kind==Kind::Word?j.word.address():j.address.address());push(child);}
        else if(pushFault){Job child;child.kind=Kind::Fault;child.fault=F(true);push(child);}
        else if(pushLatch){Job child;child.kind=Kind::Fault;child.fault=F(false);push(child);}
        else if(done){--depth_;if(depth_){auto&p=jobs_[depth_-1];
            if(p.kind==Kind::Word){if(p.word.awaitingAddress())p.word.resumeAddress();else p.word.resumeFault();}
            else if(p.kind==Kind::Address)p.address.resumeFault();
            else if(p.kind==Kind::Fault)p.fault.resumeLatch();
        }}
        return {clocks+arb,!active(),Block::None};
    }
private:
    struct Range{uint32_t begin,end;};inline static constexpr std::array<Range,3>ranges_{{{0x14e6,0x14ec},{0x15a6,0x17ba},{0x20f6,0x2104}}};
    static int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
    static unsigned cost(const Job&j){constexpr DspTransferCredits c{};
        if(j.kind==Kind::Word)return j.word.nextClocks(c);
        if(j.kind==Kind::Address)return j.address.nextClocks(c);
        if(j.kind==Kind::Fault)return j.fault.nextClocks(c);
        if(j.kind==Kind::Invalidate){constexpr unsigned v[]{9,6,12,7};return j.setup<4?v[j.setup]:0;}
        return j.setup==0?12:j.setup==1?10:j.setup==2?7:0;
    }
    bool begin(Kind k){if(!bound_||active())return false;jobs_[0]=Job{};jobs_[0].kind=k;depth_=1;value_=0;return true;}
    void push(const Job&j){if(depth_==jobs_.size())throw std::logic_error("K domain DSP capacity");jobs_[depth_++]=j;}
    ControllerDeviceState*bound_=nullptr;std::array<Job,12>jobs_{};unsigned depth_=0;uint32_t value_=0;
};
}
