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
    check(a.getPC()==b.getPC(),"part-control call PC projection differs");
    check(a.cycles==b.cycles,"part-control call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"part-control call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("part-control call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"part-control call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"part-control call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original part-control fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original part-control fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original part-control fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("part-control call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Part fixture linked synchronized H8/recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b535043);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0xa700> first{};
 auto verify=[&](unsigned entry,unsigned input,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();
  for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());a->regs[0].rl=b->regs[0].rl=int8_t(input);
  const unsigned stack=odd?0xfff001u:0xa18000u;a->setSP(stack);b->setSP(stack);
  for(auto* m:{a->memory,b->memory}){m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;m[0x174a7]=m[0x17526]=uint8_t(input);for(unsigned at=0;at<20;++at)m[0x174ae + 20*uint8_t(input)+at]=uint8_t(input+at*37);}
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"entry="<<std::hex<<entry<<" before="<<before<<std::dec<<" input="<<input<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count==2){shared.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){m[0x174a7]=m[0x17526]=uint8_t(input+count*13);for(unsigned at=0;at<20;++at)m[0x174ae + 20*uint8_t(input)+at]=uint8_t(input+at*37+count*17);}
   step();check(++count<256,"Native part control stalled");
   if(nested&&count==2){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-128;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->pc=a->makepc(entry);b->pc=b->makepc(entry);unsigned childCount=0;
    do{step();check(++childCount<256,"Nested part control stalled");if(childCount==2){shared.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=0x4338);
  if(!injected&&!nested){const unsigned expected=entry==0x4000?70:entry==0xa670?47:entry==0xa67e?497:59;check(a->cycles==1000+expected,"Part duration differs");}
  check(!b->interpretedSteps&&!shared.noteActions.partControlCalls.partialEntries&&!shared.voiceActions.partControlCalls.partialEntries,"Native part control used recovery/interpreter");++cases;if(injected)++irqCases;return count;
 };
 const std::array<unsigned,5> entries{{0x4000,0xa656,0xa66a,0xa670,0xa67e}};
 for(bool clockOnly:{false,true})for(unsigned entry:entries){
  for(unsigned input=0;input<65536;++input)verify(entry,input&255,input>>8,clockOnly,bool(input&1),unsigned(-1));
  for(unsigned input:{0u,1u,4u,5u,127u,128u,255u})verify(entry,input,0xd0,clockOnly,bool(input&1),unsigned(-1),true);
  first.fill(0);verify(entry,5,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
  for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::PartControlCalls::candidate(pc))verify(entry,5,0xe7,clockOnly,true,selected[pc]-1);
  verify(entry,5,0xd0,clockOnly,true,unsigned(-1),false,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Part control final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual native part lookup/mirror calls, "<<steps<<" independent original steps; every index x256 flags, mapped/clock-only fetch and odd/even SRAM, live source/index mutation including self-overlap; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
