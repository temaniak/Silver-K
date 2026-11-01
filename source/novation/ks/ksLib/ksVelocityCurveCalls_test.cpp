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
    check(a.getPC()==b.getPC(),"velocity-curve call PC projection differs");
    check(a.cycles==b.cycles,"velocity-curve call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"velocity-curve call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("velocity-curve call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"velocity-curve call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"velocity-curve call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original velocity-curve fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original velocity-curve fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original velocity-curve fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("velocity-curve call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Velocity fixture linked synchronized H8/recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b535643);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0xa700> first{};
 auto verify=[&](unsigned velocity,unsigned curve,unsigned power,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,bool empty=false){
  shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  a->regs[1].rh=b->regs[1].rh=int8_t(velocity);a->regs[2].rl=b->regs[2].rl=int8_t(curve);a->regs[2].rh=b->regs[2].rh=int8_t(power);
  const unsigned stack=odd?0xfff001u:0xa18000u;a->setSP(stack);b->setSP(stack);
  for(auto* m:{a->memory,b->memory}){m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;for(unsigned at=0;at<252;++at)m[0x1534e + at]=uint8_t(velocity*37+curve*11+power*17+at*7);}
  const unsigned entry=empty?0xa654:0xa5fc;a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" velocity="<<velocity<<" curve="<<curve<<" power="<<power<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count==2){shared.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory})for(unsigned at=0;at<252;++at)m[0x1534e + at]=uint8_t(velocity*37+curve*11+power*17+at*7+count*13);
   step();check(++count<2048,"Native velocity shaping stalled");
   if(nested&&count==2){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-128;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->pc=a->makepc(entry);b->pc=b->makepc(entry);unsigned childCount=0;
    do{step();check(++childCount<2048,"Nested velocity shaping stalled");if(childCount==2){shared.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=0x4338);
  if(!injected&&!nested&&!mutate){unsigned expected=velocity;const unsigned rounds=power?power:256;for(unsigned n=1;n<rounds;++n)expected=((expected*velocity)&0x7fff)>>7;const unsigned inverse=expected^0x7f;bool suppress=false;
   if(curve==1)expected=inverse;else if(curve==2)expected=((expected*expected)&0x7fff)>>7;else if(curve==3)expected=((expected*inverse)&0x7fff)>>7;else if(curve>=4){const unsigned threshold=uint8_t(velocity*37+curve*11+power*17+(curve-4)*7);suppress=threshold&1?velocity<threshold:velocity>threshold;if(suppress)expected=0x80;}
   const unsigned extra=curve==0?0:curve==1?9:curve==2?33:curve==3?36:54+(suppress?3:0);check(a->cycles==1000+(empty?17:38+30*(rounds-1)+extra),"Velocity duration differs");if(!empty)check(uint8_t(a->regs[1].rh)==expected,"Independent original velocity result differs");
  }
  check(!b->interpretedSteps&&!shared.voiceActions.velocityCurveCalls.partialEntries,"Native velocity used recovery/interpreter");++cases;if(injected)++irqCases;return count;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned velocity=0;velocity<256;++velocity)for(unsigned curve=0;curve<256;++curve)for(unsigned power:{1u,2u,3u})verify(velocity,curve,power,(velocity*149+curve*37+power)&255,clockOnly,bool(velocity&1),unsigned(-1));
  for(unsigned velocity=0;velocity<256;++velocity)for(unsigned power=0;power<256;++power)verify(velocity,(velocity+power)&255,power,(velocity+power*149)&255,clockOnly,bool(power&1),unsigned(-1));
  for(unsigned flags=0;flags<256;++flags){verify(127,7,3,flags,clockOnly,bool(flags&1),unsigned(-1));verify(0,0,1,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,true);}
  for(unsigned curve:{0u,1u,2u,3u,4u,5u,255u}){
   for(unsigned power:{0u,1u,2u,127u,128u,255u})verify(255,curve,power,0xd0,clockOnly,bool(power&1),unsigned(-1),true);
   first.fill(0);verify(127,curve,2,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::VelocityCurveCalls::candidate(pc))verify(127,curve,2,0xe7,clockOnly,true,selected[pc]-1);
   verify(127,curve,2,0xd0,clockOnly,true,unsigned(-1),false,true);
  }
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Velocity final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual native velocity calls, "<<steps<<" independent original steps; all velocity/curve pairs, all velocity/power pairs, all flags, mapped/clock-only fetch, odd/even SRAM, mutable thresholds and empty entry; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
