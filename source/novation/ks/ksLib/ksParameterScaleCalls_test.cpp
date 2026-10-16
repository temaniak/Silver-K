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
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
using Access=std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>;
struct Original final:H8SEmulator {using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
struct OriginalBus final:H8SDevice {
    std::vector<Access> events;
    uint8_t read(uint32_t a)override{const auto v=state->memory[a];events.emplace_back(false,a,v,state->cycles,uint32_t(state->getPC()));return v;}
    void write(uint32_t a,uint8_t v)override{events.emplace_back(true,a,v,state->cycles,uint32_t(state->getPC()));state->memory[a]=v;}
};
struct NativeBus final:ks::ControllerBus {
    std::vector<Access> events;
    uint8_t read(uint32_t a)override{const auto v=state->memory[a];events.emplace_back(false,a,v,state->cycles,uint32_t(state->getPC()));return v;}
    void write(uint32_t a,uint8_t v)override{events.emplace_back(true,a,v,state->cycles,uint32_t(state->getPC()));state->memory[a]=v;}
};
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"Coefficient call PC projection differs");
    check(a.cycles==b.cycles,"Coefficient call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"Coefficient call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("Coefficient call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"Coefficient call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"Coefficient call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original coefficient fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original coefficient fetch not paired");
        const unsigned bytes=std::get<1>(*first)==0xa6a4?4u:2u;
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original coefficient fetch shape differs");
        x.events.erase(first,first+bytes); // Independently admitted original CMP is four bytes.
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("Coefficient call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
    check(!ks::controllerReferenceAvailable(),"Coefficient call fixture linked the synchronized H8 adapter");
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();
    for(unsigned address:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),address);b->loadmem(os.data().data(),unsigned(os.data().size()),address);}
    ks::LiveMidiController shared(*b);ks::RuntimeController controller(*b,shared);
    OriginalBus x;NativeBus y;x.events.reserve(32);y.events.reserve(32);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
    // Admit IRQ save/RTE/resume using a synthetic vector to the actual RTE;
    // full real-handler musical behavior is checked by the Board timelines.
    for(auto* memory:{a->memory,b->memory}){memory[28*4]=memory[28*4+1]=0;memory[28*4+2]=1;memory[28*4+3]=0x70;}
    std::mt19937 rng(0x4b534346);uint64_t cases=0,boundaries=0,irqCases=0,metricResets=0,nestedCalls=0,nestedSteps=0;unsigned maximumLive=1;
    constexpr unsigned returned=0x4338;
    struct Context {std::array<int32_t,8> values{};uint32_t pc;uint8_t ccr,exr;int indent;uint64_t pending,high;};
    auto capture=[](auto& cpu){Context v;for(unsigned i=0;i<8;++i)v.values[i]=cpu.regs[i].er;v.pc=uint32_t(cpu.getPC());v.ccr=cpu.ccr;v.exr=cpu.exr;v.indent=cpu.indent;v.pending=cpu.pending_irqs;v.high=cpu.pending_irqs_high;return v;};
    auto restore=[](auto& cpu,const Context& v){for(unsigned i=0;i<8;++i)cpu.regs[i].er=v.values[i];cpu.pc=cpu.makepc(v.pc);cpu.ccr=v.ccr;cpu.exr=v.exr;cpu.indent=v.indent;cpu.pending_irqs=v.pending;cpu.pending_irqs_high=v.high;};
    auto setReturn=[&](unsigned stack){for(auto* memory:{a->memory,b->memory}){memory[stack]=memory[stack+1]=0;memory[stack+2]=uint8_t(returned>>8);memory[stack+3]=uint8_t(returned);}};
    auto step=[&](bool clockOnly){x.events.clear();y.events.clear();controller.step(*b);a->step();same(*a,*b,x,y,clockOnly);};
    auto verify=[&](unsigned bits,unsigned upper,bool clockOnly,unsigned preemptAfter,bool interleave){
        for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
        a->regs[0].er=b->regs[0].er=int32_t((rng()&0xffff0000u)|bits);
        const unsigned stack=(bits&1)?0xfff001:0xa18000;
        a->setSP(stack);b->setSP(stack);setReturn(stack);a->pc=a->makepc(0xa69e);b->pc=b->makepc(0xa69e);
        a->ccr=b->ccr=uint8_t(upper|(bits&0x2f));a->exr=b->exr=0;a->cycles=b->cycles=1000;
        a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;
        a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
        for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;
        b->nativeFetchClocksOnly=clockOnly;shared.parameterScaleCalls.discardCalls();shared.resetMetrics();
        auto nested=[&](auto&& self,unsigned depth)->void{
            const auto original=capture(*a),native=capture(*b);const unsigned childStack=a->getSP()-64;
            a->regs[0].r=b->regs[0].r=int16_t(bits+depth*7919u);a->setSP(childStack);b->setSP(childStack);setReturn(childStack);
            a->pc=a->makepc(0xa69e);b->pc=b->makepc(0xa69e);a->exr=b->exr=uint8_t(depth);a->indent=b->indent=int(depth);
            a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;
            maximumLive=std::max(maximumLive,depth+1);unsigned count=0;
            do{step(clockOnly);++nestedSteps;check(++count<256,"Nested coefficient call stalled");
                if(count==5){shared.resetMetrics();++metricResets;if(depth<7)self(self,depth+1);}
            }while(a->getPC()!=returned);
            ++nestedCalls;
            // Explicit caller-context interleave: do not claim ISR prologue
            // timing. Global clock and bus history continue across each child.
            restore(*a,original);restore(*b,native);
        };
        bool injected=false,wentDeeper=false;unsigned body=0,count=0;
        do{
            const bool irq=!injected && preemptAfter!=unsigned(-1) && body==preemptAfter;
            if(irq){injected=true;a->pending_irqs=b->pending_irqs=1ull<<28;}
            if(body==5 && !(bits&15)){shared.resetMetrics();++metricResets;}
            const unsigned before=unsigned(a->getPC());
            try{step(clockOnly);}catch(const std::exception&){std::cerr<<"input="<<int16_t(bits)<<" flags="<<upper<<" before="<<std::hex<<before<<" after="<<a->getPC()<<std::dec<<" clock-only="<<clockOnly<<" irq-at="<<preemptAfter<<'\n';throw;}
            if(!irq)++body;
            if(interleave && body==5 && !wentDeeper){wentDeeper=true;nested(nested,1);}
            check(++count<256,"Native coefficient call stalled");++boundaries;
        }while(a->getPC()!=returned);
        check(a->regs[0].er==ks::decodeParameterScale(int16_t(bits)).coefficient,"Actual coefficient output differs from native value");
        check(!b->interpretedSteps && !shared.parameterScaleCalls.partialEntries,"Native coefficient call used H8 or partial-test recovery");
        ++cases;if(injected)++irqCases;return body;
    };
    for(unsigned bits=0;bits<65536;++bits)for(unsigned upper:{0u,0x10u,0x40u,0x50u,0x80u,0x90u,0xc0u,0xd0u})for(bool clockOnly:{false,true})verify(bits,upper,clockOnly,unsigned(-1),false);
    for(int input:{0,1,1023,1024,11264,12288,32767,-1,-1024,-11264,-32767,-32768})for(bool clockOnly:{false,true}){
        const unsigned bits=uint16_t(input);const auto count=verify(bits,0xd0,clockOnly,unsigned(-1),false);
        for(unsigned at=0;at<count;++at)verify(bits,0xd0,clockOnly,at,false);
        verify(bits,0x90,clockOnly,unsigned(-1),true);
    }
    check(std::memcmp(a->memory,b->memory,1<<24)==0,"Native coefficient final full memory differs");
    std::cout<<"PASS: "<<cases<<" real runtime coefficient calls, "<<boundaries<<" independent original H8 steps; all 65536 inputs, eight masks, mapped/clock-only fetch, odd/even SRAM returns and every projected state/data/return boundary exact; "<<irqCases<<" IRQ save/RTE/resume points, "<<metricResets<<" in-flight metric resets; "<<nestedCalls<<" interleaved child calls / "<<nestedSteps<<" child steps, "<<maximumLive<<" simultaneously live tasks; no synchronized H8 or partial-test factory linked, zero native H8 steps\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
