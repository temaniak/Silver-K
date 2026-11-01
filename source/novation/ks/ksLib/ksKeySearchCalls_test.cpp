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
    check(a.getPC()==b.getPC(),"Key call PC projection differs");
    check(a.cycles==b.cycles,"Key call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"Key call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("Key call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"Key call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"Key call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original key fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original key fetch not paired");
        x.events.erase(first,first+2); // Every admitted key/RTE boundary is 2 bytes.
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("Key call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
    check(!ks::controllerReferenceAvailable(),"Actual production key-call test linked the synchronized H8 adapter");
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();
    for(unsigned address:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),address);b->loadmem(os.data().data(),unsigned(os.data().size()),address);}
    ks::LiveMidiController shared(*b);ks::RuntimeController controller(*b,shared);
    OriginalBus x;NativeBus y;x.events.reserve(32);y.events.reserve(32);
    a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
    // A synthetic vector points to the independently shipped RTE. This admits
    // exact IRQ entry/save/return around the native task, not the real handler's
    // complete musical behavior (the full Board tests cover those handlers).
    a->memory[28*4+2]=b->memory[28*4+2]=1;a->memory[28*4+3]=b->memory[28*4+3]=0x70;
    a->memory[28*4]=b->memory[28*4]=0;a->memory[28*4+1]=b->memory[28*4+1]=0;
    using Keys=std::array<uint8_t,48>;std::vector<Keys> inputs(1);Keys full{};full.fill(255);inputs.push_back(full);
    for(unsigned bit=0;bit<384;++bit){Keys single{};single[bit/8]=uint8_t(1u<<(bit%8));inputs.push_back(single);}
    std::mt19937 rng(0x4b53434c);for(unsigned n=0;n<64;++n){Keys v{};for(auto& x:v)x=uint8_t(rng());inputs.push_back(v);}
    uint64_t cases=0,boundaries=0,irqCases=0,metricResets=0;
    constexpr unsigned stack=0xfff000,returned=0x4338;
    auto verify=[&](const Keys& keys,unsigned key,bool next,bool clockOnly,unsigned base,unsigned preemptAfter){
        Keys active=keys;std::copy(active.begin(),active.end(),a->memory+base);std::copy(active.begin(),active.end(),b->memory+base);
        for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
        a->regs[3].er=b->regs[3].er=int32_t(base);a->regs[5].rl=b->regs[5].rl=int8_t(key);
        a->setSP(stack);b->setSP(stack);a->pc=a->makepc(next?0x317a:0x31ac);b->pc=b->makepc(next?0x317a:0x31ac);
        a->memory[stack]=b->memory[stack]=0;a->memory[stack+1]=b->memory[stack+1]=0;
        a->memory[stack+2]=b->memory[stack+2]=uint8_t(returned>>8);a->memory[stack+3]=b->memory[stack+3]=uint8_t(returned);
        a->ccr=b->ccr=uint8_t(rng());a->exr=b->exr=0;a->cycles=b->cycles=1000;
        a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;
        a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
        for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;
        a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
        shared.noteActions.discardTransientCalls();shared.resetMetrics();
        bool injected=false;unsigned body=0,steps=0;
        do{
            const bool interrupt=!injected && preemptAfter!=unsigned(-1) && body==preemptAfter;
            if(interrupt){
                a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;
                for(unsigned i=0;i<active.size();++i)active[i]^=uint8_t(0x81u+(i*13u+key)*7u);
                std::copy(active.begin(),active.end(),a->memory+base);std::copy(active.begin(),active.end(),b->memory+base);
            }
            // Starting diagnostics during a suspended native call must reset
            // counters, not throw away its functional continuation.
            if(body==17 && !(key&15)){shared.resetMetrics();++metricResets;}
            x.events.clear();y.events.clear();const unsigned before=unsigned(a->getPC());
            controller.step(*b);a->step();
            try{same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"key="<<key<<" next="<<next<<" before="<<std::hex<<before<<" after="<<a->getPC()<<" base="<<base<<std::dec<<" clock-only="<<clockOnly<<" irq-at="<<preemptAfter<<'\n';throw;}
            if(!interrupt)++body;
            check(++steps<4096,"Native key call did not return");++boundaries;
        }while(a->getPC()!=returned);
        check(b->interpretedSteps==0 && shared.noteActions.keySearchCalls.partialEntries==0,"Native key call used H8 or a test partial frame");
        check(std::equal(active.begin(),active.end(),a->memory+base) && std::equal(active.begin(),active.end(),b->memory+base),"Key call changed key region");
        ++cases;if(injected)++irqCases;return body;
    };
    for(unsigned base:{0x16192u,0xa16192u})for(const auto& keys:inputs)for(unsigned key=0;key<256;++key)
        for(bool next:{false,true})for(bool clockOnly:{false,true})verify(keys,key,next,clockOnly,base,unsigned(-1));
    for(unsigned pattern:{0u,1u,2u,65u,129u,unsigned(inputs.size()-1)})for(unsigned key:{0u,7u,8u,63u,127u,128u,248u,255u})for(bool next:{false,true}){
        const unsigned body=verify(inputs[pattern],key,next,true,0xa16192,unsigned(-1));
        for(unsigned at=0;at<body;++at)verify(inputs[pattern],key,next,true,0xa16192,at);
    }
    check(std::memcmp(a->memory,b->memory,1<<24)==0,"Native key call final full memory differs");
    std::cout<<"PASS: "<<cases<<" actual runtime native key calls, "<<boundaries<<" independently executed H8 steps; full 8-bit input domain, live SRAM and low alias, mapped/clock-only fetch, all projected state/data/return accesses exact; "<<irqCases<<" interrupt save/RTE/resume + live-key mutation points and "<<metricResets<<" in-flight metric resets; zero native H8 steps, no test fallback linked\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
