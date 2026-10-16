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
    std::array<unsigned,2> reads{},busy{};
    unsigned writes=0,dataReads=0,phase=0,base=0x400000;
    uint16_t address=0;
    uint32_t word=0;
    bool receive=false;
};
template<class Device>struct MockBus final:Device {
    Script script;
    std::vector<Access> events;
    uint8_t read(uint32_t a)override{
        uint8_t v=this->state->memory[a];
        if(a==0x400004 || a==0x600004){
            check(a==script.base+4,"Wrong DSP status port");
            const unsigned mask=script.phase && script.receive?1:4;
            v=script.reads[script.phase]++<script.busy[script.phase]?uint8_t(255&~mask):255;
        }else if((a>=0x40000a && a<=0x40000e)||(a>=0x60000a && a<=0x60000e)){
            check(script.receive && script.writes==5 && script.dataReads<3 && a==script.base+10+2*script.dataReads,"Unexpected DSP data read");
            v=uint8_t(script.word>>(16-8*script.dataReads++));
        }
        events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;
    }
    void write(uint32_t a,uint8_t v)override{
        if((a>=0x400000 && a<=0x40000e)||(a>=0x600000 && a<=0x60000e)){
            constexpr unsigned offsets[]{0,10,12,14,0};
            const uint8_t values[]{8,uint8_t(script.receive?0x80:0),uint8_t(script.address>>8),uint8_t(script.address),0};
            check(script.writes<5 && a==script.base+offsets[script.writes] && v==values[script.writes],"Native DSP request byte/order/value differs");
            if(++script.writes==4)script.phase=1;
        }
        events.emplace_back(true,a,v,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[a]=v;
    }
};
using OriginalBus=MockBus<H8SDevice>;
using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"DSP-address call PC projection differs");
    check(a.cycles==b.cycles,"DSP-address call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"DSP-address call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("DSP-address call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"DSP-address call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"DSP-address call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original DSP-address fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original DSP-address fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original DSP-address fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("DSP-address call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
    check(!ks::controllerReferenceAvailable(),"DSP-address fixture linked synchronized H8/recovery adapter");
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();
    lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
    for(unsigned address:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),address);b->loadmem(os.data().data(),unsigned(os.data().size()),address);}
    ks::LiveMidiController shared(*b);ks::RuntimeController controller(*b,shared);
    for(auto* memory:{a->memory,b->memory}){memory[28*4]=memory[28*4+1]=0;memory[28*4+2]=1;memory[28*4+3]=0x70;}
    OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
    std::mt19937 rng(0x4b534144);uint64_t cases=0,steps=0,irqCases=0,resets=0,faults=0,nestedCalls=0,nestedSteps=0;unsigned maximumLive=1;
    constexpr unsigned returned=0x4338;
    struct Context {std::array<int32_t,8> values{};uint32_t pc;uint8_t ccr,exr;int indent;uint64_t pending,high;};
    auto capture=[](auto& cpu){Context v;for(unsigned i=0;i<8;++i)v.values[i]=cpu.regs[i].er;v.pc=uint32_t(cpu.getPC());v.ccr=cpu.ccr;v.exr=cpu.exr;v.indent=cpu.indent;v.pending=cpu.pending_irqs;v.high=cpu.pending_irqs_high;return v;};
    auto restore=[](auto& cpu,const Context& v){for(unsigned i=0;i<8;++i)cpu.regs[i].er=v.values[i];cpu.pc=cpu.makepc(v.pc);cpu.ccr=v.ccr;cpu.exr=v.exr;cpu.indent=v.indent;cpu.pending_irqs=v.pending;cpu.pending_irqs_high=v.high;};
    auto setReturn=[&](unsigned stack){for(auto* memory:{a->memory,b->memory}){memory[stack]=memory[stack+1]=0;memory[stack+2]=uint8_t(returned>>8);memory[stack+3]=uint8_t(returned);}};
    auto verify=[&](uint32_t word,unsigned address,unsigned base,unsigned entry,unsigned flags,unsigned busyFirst,unsigned busySecond,bool clockOnly,unsigned preempt,bool interleave=false){
        shared.voiceActions.discardTransientCalls();shared.resetMetrics();
        for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
        a->regs[4].r=b->regs[4].r=int16_t(address);
        a->regs[5].er=b->regs[5].er=(entry==0x2560 || entry==0x2568)?int32_t(rng()):int32_t(base);
        const unsigned stack=(address&1)?0xfff001:0xa18000;a->setSP(stack);b->setSP(stack);setReturn(stack);
        a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
        a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;
        a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
        for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;
        b->nativeFetchClocksOnly=clockOnly;
        Script script;script.busy={busyFirst,busySecond};script.base=base;script.address=uint16_t(address);script.word=word&0xffffff;script.receive=entry!=0x2416;x.script=y.script=script;
        auto step=[&]{x.events.clear();y.events.clear();controller.step(*b);a->step();same(*a,*b,x,y,clockOnly);};
        auto nested=[&](auto&& self,unsigned depth)->void{
            const auto original=capture(*a),native=capture(*b);const auto sx=x.script,sy=y.script;
            const unsigned childStack=a->getSP()-64;a->setSP(childStack);b->setSP(childStack);setReturn(childStack);
            a->regs[4].r=b->regs[4].r=int16_t(0x7fff+depth);a->regs[5].er=b->regs[5].er=0x600000;
            a->pc=a->makepc(depth&1?0x2568:0x2416);b->pc=b->makepc(depth&1?0x2568:0x2416);a->exr=b->exr=uint8_t(depth);a->indent=b->indent=int(depth);
            a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;
            Script child;child.base=depth&1?0x400000:0x600000;child.address=uint16_t(0x7fff+depth);child.receive=depth&1;child.word=0x654321+depth;child.busy={depth%4,depth%3};x.script=y.script=child;
            maximumLive=std::max(maximumLive,depth+1);unsigned count=0;
            do{step();++nestedSteps;check(++count<256,"Nested DSP-address call stalled");if(count==5){shared.resetMetrics();++resets;if(depth<7)self(self,depth+1);}}while(a->getPC()!=returned);
            check(x.script.reads==y.script.reads && x.script.writes==5 && y.script.writes==5,"Nested native address bus differs");++nestedCalls;
            // Explicit caller interleave is separate from IRQ prologue timing.
            restore(*a,original);restore(*b,native);x.script=sx;y.script=sy;
        };
        unsigned count=0;bool injected=false;
        do{
            if(count==preempt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
            if(count==5){shared.resetMetrics();++resets;}
            const unsigned before=unsigned(a->getPC());if(before==0x2304)++faults;
            try{step();}catch(const std::exception&){std::cerr<<"word="<<std::hex<<word<<" entry="<<entry<<" before="<<before<<std::dec<<" busy="<<busyFirst<<','<<busySecond<<" clock-only="<<clockOnly<<'\n';throw;}
            check(++count<3000000,"Actual native DSP-address task stalled");++steps;
            if(interleave && count==5)nested(nested,1);
        }while(a->getPC()!=returned);
        check(x.script.reads==std::array<unsigned,2>{busyFirst+1,busySecond+1} && y.script.reads==x.script.reads && x.script.writes==5 && y.script.writes==5,"Actual readiness/request counts differ");
        check(!shared.faultCalls.partialEntries,"Native fault used partial-test recovery");
        const uint64_t expectedFaults=(busyFirst?uint64_t(busyFirst-1)/65536:0)+(busySecond?uint64_t(busySecond-1)/65536:0);
        check(shared.faultCalls.started==expectedFaults && shared.faultCalls.completed==expectedFaults,"Address timeout did not execute complete native fault jobs");
        check(!b->interpretedSteps && !shared.voiceActions.addressCalls.partialEntries,"Native address task used interpreter/test recovery");
        if(script.receive)check(x.script.dataReads==3 && y.script.dataReads==3 && uint16_t(a->regs[4].r)==uint16_t(word>>8) && uint8_t(a->regs[0].rl)==uint8_t(word),"Actual read result differs from native word");
        else check(uint16_t(a->regs[4].r)==uint16_t(address),"Address-only request changed address");
        ++cases;if(injected)++irqCases;return count;
    };
    for(bool clockOnly:{false,true})for(auto variant:{std::pair<unsigned,unsigned>{0x2416,0x400000},{0x2416,0x600000},{0x256e,0x400000},{0x256e,0x600000},{0x2560,0x600000},{0x2568,0x400000}}){
        const auto entry=variant.first,base=variant.second;
        for(unsigned position=0;position<3;++position)for(unsigned byte=0;byte<256;++byte)
        for(unsigned flags:{0u,0x10u,0x40u,0x50u,0x80u,0x90u,0xc0u,0xd0u})for(unsigned busy:{0u,1u,2u,7u})
            verify(0xa55ac3u^(byte<<(8*position)),uint16_t(rng()),base,entry,flags|unsigned(rng()&47),busy,7-busy,clockOnly,unsigned(-1));
        for(unsigned busy:{0u,2u}){
            const unsigned count=verify(0x123456,0xffff,base,entry,0xd0,busy,busy,clockOnly,unsigned(-1));
            for(unsigned at=0;at<count;++at)verify(0x123456,0xffff,base,entry,0xd0,busy,busy,clockOnly,at);
        }
        verify(0xfedcba,0xffff,base,entry,0xd0,2,1,clockOnly,unsigned(-1),true);
        for(unsigned busy:{65537u,131073u})for(bool second:{false,true})verify(0xabcdef,0xffff,base,entry,0xd0,second?0:busy,second?busy:0,clockOnly,unsigned(-1));
    }
    for(unsigned address=0;address<65536;++address)for(bool clockOnly:{false,true})for(unsigned base:{0x400000u,0x600000u})for(unsigned entry:{0x2416u,0x256eu})
        verify(0xff0080,address,base,entry,0xd0,0,0,clockOnly,unsigned(-1));
    check(std::memcmp(a->memory,b->memory,1<<24)==0,"Native DSP-address final full memory differs");
    std::cout<<"PASS: "<<cases<<" real runtime DSP-address/read calls, "<<steps<<" independent original H8 steps; all addresses, reply byte positions, both DSP ports/selectors, live dual-phase readiness/retry/timeout and actual GPIO fault service, masks and mapped/clock-only fetch exact; "<<irqCases<<" IRQ save/RTE/resume points, "<<resets<<" in-flight metric resets, "<<faults<<" original fault-service entries, "<<nestedCalls<<" interleaved child calls / "<<nestedSteps<<" child steps, "<<maximumLive<<" simultaneously live tasks; no synchronized H8 or partial-test factory linked\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
