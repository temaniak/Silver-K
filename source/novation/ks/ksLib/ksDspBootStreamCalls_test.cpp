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
struct Script {
    uint32_t source=0x20000,words=1,base=0x400000,seed=0,version=0;
    uint32_t sourceReads=0,writes=0,readsInWord=0,busyFirst=0,busyLater=0;
    uint64_t statusReads=0;
    uint8_t latched=0;
};
template<class Device>struct MockBus final:Device {
    Script script;
    std::vector<Access> events;
    uint8_t read(uint32_t a)override{
        uint8_t value=this->state->memory[a];
        if(a==0x400004 || a==0x600004){
            check(a==script.base+4,"Wrong bootstrap DSP status port");const auto busy=script.writes<3?script.busyFirst:script.busyLater;
            value=script.readsInWord++<busy?0xfd:0xff;++script.statusReads;
        }else if(a>=script.source && a<script.source+script.words*3){
            check(a==script.source+script.sourceReads && script.sourceReads==script.writes,"Firmware source prefetched/reordered");
            const auto offset=script.sourceReads++;value=uint8_t(script.seed+offset*37+script.version*13+(offset>>8));script.latched=value;
        }
        events.emplace_back(false,a,value,this->state->cycles,uint32_t(this->state->getPC()));return value;
    }
    void write(uint32_t a,uint8_t value)override{
        if((a>=0x400000 && a<0x400010)||(a>=0x600000 && a<0x600010)){
            check(a==script.base+10+2*(script.writes%3) && script.sourceReads==script.writes+1 && value==script.latched,"Firmware stream output byte/order differs");
            if(!(++script.writes%3))script.readsInWord=0;
        }
        events.emplace_back(true,a,value,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[a]=value;
    }
};
using OriginalBus=MockBus<H8SDevice>;
using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"DSP-bootstrap call PC projection differs");
    check(a.cycles==b.cycles,"DSP-bootstrap call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"DSP-bootstrap call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("DSP-bootstrap call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"DSP-bootstrap call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"DSP-bootstrap call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original DSP-bootstrap fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original DSP-bootstrap fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original DSP-bootstrap fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("DSP-bootstrap call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
    check(!ks::controllerReferenceAvailable(),"Bootstrap fixture linked synchronized H8/recovery adapter");
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();
    lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
    for(unsigned address:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),address);b->loadmem(os.data().data(),unsigned(os.data().size()),address);}
    ks::LiveMidiController shared(*b);ks::RuntimeController controller(*b,shared);
    for(auto* memory:{a->memory,b->memory}){memory[28*4]=memory[28*4+1]=0;memory[28*4+2]=1;memory[28*4+3]=0x70;}
    OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
    std::mt19937 rng(0x4b534253);uint64_t cases=0,steps=0,irqCases=0,resets=0,faults=0,nestedCalls=0,nestedSteps=0;unsigned maximumLive=1;
    constexpr unsigned returned=0x4338;
    struct Context {std::array<int32_t,8> values{};uint32_t pc;uint8_t ccr,exr;int indent;uint64_t pending,high;};
    auto capture=[](auto& cpu){Context v;for(unsigned i=0;i<8;++i)v.values[i]=cpu.regs[i].er;v.pc=uint32_t(cpu.getPC());v.ccr=cpu.ccr;v.exr=cpu.exr;v.indent=cpu.indent;v.pending=cpu.pending_irqs;v.high=cpu.pending_irqs_high;return v;};
    auto restore=[](auto& cpu,const Context& v){for(unsigned i=0;i<8;++i)cpu.regs[i].er=v.values[i];cpu.pc=cpu.makepc(v.pc);cpu.ccr=v.ccr;cpu.exr=v.exr;cpu.indent=v.indent;cpu.pending_irqs=v.pending;cpu.pending_irqs_high=v.high;};
    auto setReturn=[&](unsigned stack){for(auto* memory:{a->memory,b->memory}){memory[stack]=memory[stack+1]=0;memory[stack+2]=uint8_t(returned>>8);memory[stack+3]=uint8_t(returned);}};
    auto verify=[&](uint16_t input,unsigned seed,unsigned flags,bool clockOnly,bool odd,unsigned port,uint32_t source,unsigned firstBusy,unsigned laterBusy,unsigned preempt,bool interleave=false){
        shared.bootStreamCalls.discardCalls();shared.faultCalls.discardCalls();shared.resetMetrics();
        for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
        a->regs[3].e=b->regs[3].e=int16_t(input);a->regs[4].er=b->regs[4].er=int32_t(source);a->regs[5].er=b->regs[5].er=int32_t(port);
        const unsigned stack=odd?0xfff001:0xa18000;a->setSP(stack);b->setSP(stack);setReturn(stack);
        a->pc=a->makepc(0x23ce);b->pc=b->makepc(0x23ce);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
        a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;
        a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
        for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;
        b->nativeFetchClocksOnly=clockOnly;
        for(auto* memory:{a->memory,b->memory}){memory[0xffec22]=uint8_t(seed);memory[0xffff61]=0xa5;memory[0xffff62]=0x5a;}
        Script script;script.source=source&0xffffff;script.words=input?input:65536u;script.base=port;script.seed=seed;script.busyFirst=firstBusy;script.busyLater=laterBusy;x.script=y.script=script;
        auto step=[&]{x.events.clear();y.events.clear();controller.step(*b);a->step();same(*a,*b,x,y,clockOnly);};
        auto nested=[&](auto&& self,unsigned depth)->void{
            const auto original=capture(*a),native=capture(*b);const auto sx=x.script,sy=y.script;
            const unsigned childStack=a->getSP()-64;a->setSP(childStack);b->setSP(childStack);setReturn(childStack);
            a->regs[3].e=b->regs[3].e=2;a->regs[4].er=b->regs[4].er=int32_t(depth&1?0xffa40001u:0x20000u);a->regs[5].er=b->regs[5].er=int32_t(depth&1?0x600000:0x400000);
            a->pc=a->makepc(0x23ce);b->pc=b->makepc(0x23ce);a->exr=b->exr=uint8_t(depth);a->indent=b->indent=int(depth);
            a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;
            Script child;child.source=depth&1?0xa40001:0x20000;child.base=depth&1?0x600000:0x400000;child.words=2;child.seed=seed+depth*37;child.busyFirst=depth%3;child.busyLater=depth%2;x.script=y.script=child;
            maximumLive=std::max(maximumLive,depth+1);unsigned count=0;
            do{x.script.version=y.script.version=count+depth*53;step();++nestedSteps;check(++count<256,"Nested bootstrap call stalled");if(count==5){shared.resetMetrics();++resets;if(depth<7)self(self,depth+1);}}while(a->getPC()!=returned);
            check(x.script.sourceReads==6 && y.script.sourceReads==6 && x.script.writes==6 && y.script.writes==6,"Nested native firmware byte count differs");++nestedCalls;
            // Explicit caller-context interleave; actual IRQ/RTE is separate.
            restore(*a,original);restore(*b,native);x.script=sx;y.script=sy;
        };
        unsigned count=0;bool injected=false;
        do{
            if(count==preempt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
            if(count==5){shared.resetMetrics();++resets;}
            x.script.version=y.script.version=count;
            const unsigned before=unsigned(a->getPC());if(before==0x2304)++faults;
            try{step();}catch(const std::exception&){std::cerr<<"words="<<input<<" source="<<std::hex<<source<<" before="<<before<<std::dec<<" busy="<<firstBusy<<','<<laterBusy<<" flags="<<flags<<" clock-only="<<clockOnly<<'\n';throw;}
            check(++count<4000000,"Actual native bootstrap stream stalled");++steps;
            if(interleave && count==5)nested(nested,1);
        }while(a->getPC()!=returned);
        const uint64_t expectedFaults=(firstBusy?uint64_t(firstBusy-1)/65536:0)+uint64_t(script.words-1)*(laterBusy?uint64_t(laterBusy-1)/65536:0);
        check(x.script.sourceReads==script.words*3 && x.script.writes==script.words*3 && y.script.sourceReads==x.script.sourceReads && y.script.writes==x.script.writes && x.script.statusReads==y.script.statusReads,"Actual firmware stream byte/readiness counts differ");
        check(x.script.statusReads==firstBusy+1+uint64_t(script.words-1)*(laterBusy+1),"Actual firmware stream observed incorrect readiness count");
        check(uint32_t(a->regs[4].er)==source+script.words*3 && !a->regs[3].e,"Actual firmware stream cursor/count result differs");
        check(!b->interpretedSteps && !shared.bootStreamCalls.partialEntries && !shared.faultCalls.partialEntries,"Native firmware stream used interpreter/test recovery");
        check(shared.faultCalls.started==expectedFaults && shared.faultCalls.completed==expectedFaults,"Firmware timeout did not execute complete native fault jobs");
        if(!injected && !interleave){
            auto delay=[](unsigned n)->uint64_t{return n?24ull+uint64_t(n-1)*27+uint64_t((n-1)/65536)*26:0;};
            const uint64_t expected=1000+uint64_t(script.words)*96+delay(firstBusy)+uint64_t(script.words-1)*delay(laterBusy)+expectedFaults*176+17;
            check(a->cycles==expected,"Actual firmware stream duration differs");
        }
        ++cases;if(injected)++irqCases;return count;
    };
    for(bool clockOnly:{false,true})for(bool odd:{false,true})for(unsigned port:{0x400000u,0x600000u})for(uint32_t source:{0x20000u,0xffa40001u}){
        for(unsigned seed=0;seed<256;++seed)for(unsigned flags:{0u,0x10u,0x40u,0x50u,0x80u,0x90u,0xc0u,0xd0u})for(unsigned busy:{0u,1u,2u,7u})
            verify(2,seed,flags|unsigned(rng()&47),clockOnly,odd,port,source,busy,7-busy,unsigned(-1));
        for(unsigned input:{0u,1u,3u,255u,256u,257u,32767u,32768u,65535u})verify(uint16_t(input),0xa5,0xd0,clockOnly,odd,port,source,0,0,unsigned(-1));
        for(unsigned busy:{0u,2u}){
            const unsigned count=verify(2,0xa5,0xd0,clockOnly,odd,port,source,busy,busy,unsigned(-1));
            for(unsigned at=0;at<count;++at)verify(2,0xa5,0xd0,clockOnly,odd,port,source,busy,busy,at);
        }
        verify(2,0x80,0xd0,clockOnly,odd,port,source,2,1,unsigned(-1),true);
        for(unsigned busy:{65536u,65537u,131073u})verify(2,0xa5,0xd0,clockOnly,odd,port,source,busy,0,unsigned(-1));
    }
    check(std::memcmp(a->memory,b->memory,1<<24)==0,"Native firmware stream final full memory differs");
    std::cout<<"PASS: "<<cases<<" real runtime firmware-stream calls, "<<steps<<" independent original H8 steps; all byte seeds, retained masks, both DSPs, low/high/tagged sources, live mutations and 16-bit count edges including65536, mapped/clock-only fetch and odd/even SRAM; "<<irqCases<<" IRQ save/RTE/resume points, "<<resets<<" metric resets, "<<faults<<" native fault-service entries, "<<nestedCalls<<" interleaved child calls / "<<nestedSteps<<" child steps, "<<maximumLive<<" live tasks; no synchronized H8 or partial-test factory linked\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
