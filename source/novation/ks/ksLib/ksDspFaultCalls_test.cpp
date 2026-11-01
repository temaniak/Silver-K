#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>
namespace {
std::unique_ptr<H8SDisassembler> lengthOracle;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
using Access=std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>;
struct Original final:H8SEmulator {using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
using Io=std::tuple<bool,uint32_t,uint8_t>;
struct Script {
    std::array<uint8_t,2> controls{};
    unsigned controlReads=0,ioCount=0;
    uint8_t observedLatch=0;
    std::array<Io,8> io{};
};
template<class Device>struct MockBus final:Device {
    Script script;
    std::vector<Access> events;
    void io(bool write,uint32_t a,uint8_t v){
        if(a!=0xffec22 && a!=0xffff61 && a!=0xffff62)return;
        check(script.ioCount<script.io.size(),"Unexpected fault GPIO operation");script.io[script.ioCount++]={write,a,v};
    }
    uint8_t read(uint32_t a)override{
        uint8_t value=this->state->memory[a];
        if(a==0xffff62){check(script.controlReads<2,"Unexpected fault control observation");value=script.controls[script.controlReads++];}
        if(a==0xffec22)script.observedLatch=value;
        io(false,a,value);events.emplace_back(false,a,value,this->state->cycles,uint32_t(this->state->getPC()));return value;
    }
    void write(uint32_t a,uint8_t value)override{
        io(true,a,value);events.emplace_back(true,a,value,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[a]=value;
    }
};
using OriginalBus=MockBus<H8SDevice>;
using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"DSP-fault call PC projection differs");
    check(a.cycles==b.cycles,"DSP-fault call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"DSP-fault call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("DSP-fault call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"DSP-fault call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"DSP-fault call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original DSP-fault fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original DSP-fault fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original DSP-fault fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("DSP-fault call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
    check(!ks::controllerReferenceAvailable(),"Fault fixture linked synchronized H8/recovery adapter");
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();
    lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
    for(unsigned address:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),address);b->loadmem(os.data().data(),unsigned(os.data().size()),address);}
    ks::LiveMidiController shared(*b);ks::RuntimeController controller(*b,shared);
    for(auto* memory:{a->memory,b->memory}){memory[28*4]=memory[28*4+1]=0;memory[28*4+2]=1;memory[28*4+3]=0x70;}
    OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
    std::mt19937 rng(0x4b534654);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCalls=0,nestedSteps=0;unsigned maximumLive=1;
    constexpr unsigned returned=0x4338;
    struct Context {std::array<int32_t,8> values{};uint32_t pc;uint8_t ccr,exr;int indent;uint64_t pending,high;};
    auto capture=[](auto& cpu){Context v;for(unsigned i=0;i<8;++i)v.values[i]=cpu.regs[i].er;v.pc=uint32_t(cpu.getPC());v.ccr=cpu.ccr;v.exr=cpu.exr;v.indent=cpu.indent;v.pending=cpu.pending_irqs;v.high=cpu.pending_irqs_high;return v;};
    auto restore=[](auto& cpu,const Context& v){for(unsigned i=0;i<8;++i)cpu.regs[i].er=v.values[i];cpu.pc=cpu.makepc(v.pc);cpu.ccr=v.ccr;cpu.exr=v.exr;cpu.indent=v.indent;cpu.pending_irqs=v.pending;cpu.pending_irqs_high=v.high;};
    auto setReturn=[&](unsigned stack){for(auto* memory:{a->memory,b->memory}){memory[stack]=memory[stack+1]=0;memory[stack+2]=uint8_t(returned>>8);memory[stack+3]=uint8_t(returned);}};
    auto ioExact=[&]{
        const uint8_t latch=uint8_t(x.script.observedLatch^7);const auto controls=x.script.controls;
        const std::array<Io,8> expected{{{false,0xffec22,x.script.observedLatch},{true,0xffec22,latch},{true,0xffff61,latch},{true,0xffff62,0x36},{false,0xffff62,controls[0]},{true,0xffff62,uint8_t(controls[0]&~0x20)},{false,0xffff62,controls[1]},{true,0xffff62,uint8_t(controls[1]|0x20)}}};
        check(x.script.io==expected && y.script.io==expected && x.script.ioCount==8 && y.script.ioCount==8 && x.script.controlReads==2 && y.script.controlReads==2,"Native fault GPIO sequence/values differ");
    };
    auto verify=[&](unsigned latch,unsigned first,unsigned last,unsigned flags,bool clockOnly,bool odd,unsigned preempt,bool interleave=false){
        shared.faultCalls.discardCalls();shared.resetMetrics();
        for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
        const unsigned stack=odd?0xfff001:0xa18000;a->setSP(stack);b->setSP(stack);setReturn(stack);
        a->pc=a->makepc(0x2304);b->pc=b->makepc(0x2304);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
        a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;
        a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
        for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;
        b->nativeFetchClocksOnly=clockOnly;
        for(auto* memory:{a->memory,b->memory}){memory[0xffec22]=uint8_t(latch);memory[0xffff61]=0xa5;memory[0xffff62]=0x5a;}
        Script script;script.controls={uint8_t(first),uint8_t(last)};x.script=y.script=script;
        auto step=[&]{x.events.clear();y.events.clear();controller.step(*b);a->step();same(*a,*b,x,y,clockOnly);};
        auto nested=[&](auto&& self,unsigned depth)->void{
            const auto original=capture(*a),native=capture(*b);const auto sx=x.script,sy=y.script;
            const unsigned childStack=a->getSP()-64;a->setSP(childStack);b->setSP(childStack);setReturn(childStack);
            a->pc=a->makepc(0x2304);b->pc=b->makepc(0x2304);a->exr=b->exr=uint8_t(depth);a->indent=b->indent=int(depth);
            a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;
            Script child;child.controls={uint8_t(first+depth*37),uint8_t(last+depth*53)};x.script=y.script=child;
            maximumLive=std::max(maximumLive,depth+1);unsigned count=0;
            do{step();++nestedSteps;check(++count<64,"Nested native fault call stalled");if(count==5){shared.resetMetrics();++resets;if(depth<7)self(self,depth+1);}}while(a->getPC()!=returned);
            ioExact();++nestedCalls;
            // Explicit caller-context interleave; actual IRQ/RTE is separate.
            restore(*a,original);restore(*b,native);x.script=sx;y.script=sy;
        };
        unsigned count=0;bool injected=false;
        do{
            if(count==preempt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
            if(count==5){shared.resetMetrics();++resets;}
            const unsigned before=unsigned(a->getPC());
            try{step();}catch(const std::exception&){std::cerr<<"latch="<<latch<<" controls="<<first<<','<<last<<" flags="<<flags<<" before="<<std::hex<<before<<std::dec<<" clock-only="<<clockOnly<<" odd="<<odd<<'\n';throw;}
            check(++count<64,"Actual native fault task stalled");++steps;
            if(interleave && count==5)nested(nested,1);
        }while(a->getPC()!=returned);
        ioExact();check(!b->interpretedSteps && !shared.faultCalls.partialEntries,"Native fault used interpreter/test recovery");
        if(!injected && !interleave)check(a->cycles==1176,"Actual native fault service duration differs");
        ++cases;if(injected)++irqCases;return count;
    };
    for(unsigned latch=0;latch<256;++latch)for(unsigned flags=0;flags<256;++flags)for(bool clockOnly:{false,true})for(bool odd:{false,true})
        verify(latch,rng()&255,rng()&255,flags,clockOnly,odd,unsigned(-1));
    for(unsigned first=0;first<256;++first)for(unsigned last=0;last<256;++last)for(bool clockOnly:{false,true})for(bool odd:{false,true})
        verify(0xa5,first,last,rng()&255,clockOnly,odd,unsigned(-1));
    for(unsigned latch:{0u,1u,7u,8u,127u,128u,254u,255u})for(bool clockOnly:{false,true})for(bool odd:{false,true}){
        const unsigned count=verify(latch,0xff,0,0xd0,clockOnly,odd,unsigned(-1));
        for(unsigned at=0;at<count;++at)verify(latch,0xff,0,0xd0,clockOnly,odd,at);
        verify(latch,0x36,0,0x90,clockOnly,odd,unsigned(-1),true);
    }
    check(std::memcmp(a->memory,b->memory,1<<24)==0,"Native fault final full memory differs");
    std::cout<<"PASS: "<<cases<<" real runtime fault calls, "<<steps<<" independent original H8 steps; every latch byte/initial flags and every control-byte pair, mapped/clock-only fetch and odd/even SRAM returns, exact GPIO/data/stack/clocks/projection; "<<irqCases<<" IRQ save/RTE/resume points, "<<resets<<" in-flight metric resets, "<<nestedCalls<<" interleaved child calls / "<<nestedSteps<<" child steps, "<<maximumLive<<" simultaneously live tasks; no synchronized H8 or partial-test factory linked\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
