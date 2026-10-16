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
template<class Device>struct MockBus final:Device {
 std::vector<Access> events;uint8_t pitch=0,middle=0x55,low=0xaa;
 uint8_t read(uint32_t a)override{uint8_t value=this->state->memory[a];if(a==0x40000a||a==0x60000a)value=pitch;else if(a==0x40000c||a==0x60000c)value=middle;else if(a==0x40000e||a==0x60000e)value=low;events.emplace_back(false,a,value,this->state->cycles,uint32_t(this->state->getPC()));return value;}
 void write(uint32_t a,uint8_t value)override{events.emplace_back(true,a,value,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[a]=value;}
};
using OriginalBus=MockBus<H8SDevice>;using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"unison call PC projection differs");
    check(a.cycles==b.cycles,"unison call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"unison call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("unison call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"unison call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"unison call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original unison fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original unison fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original unison fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("unison call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Unison fixture linked synchronized recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b53554e);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x9100> first{};
 auto store32=[](uint8_t* m,unsigned at,uint32_t value){for(unsigned i=0;i<4;++i)m[at+i]=uint8_t(value>>(24-8*i));};
 auto verify=[&](unsigned mode,unsigned unison,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned part=2,unsigned note=60,unsigned velocity=100,bool tagged=false){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  a->regs[5].rl=b->regs[5].rl=int8_t(note);a->regs[5].rh=b->regs[5].rh=int8_t(velocity);
  const unsigned stack=odd?0xfff001u:0xa18000u;const uint32_t patch=tagged?0xffa30000u:0x30000u;const unsigned physical=patch&0xffffff;
  a->setSP(stack);b->setSP(stack);for(auto* m:{a->memory,b->memory}){
   m[0x400004]=m[0x600004]=7;m[0x17526]=uint8_t(part);m[0x174a6]=14;m[0x174a7]=0xee;m[0x169f1]=0;m[0x1755e]=0xff;
   for(unsigned p=0;p<4;++p){const unsigned at=0x174ae + p*20;for(unsigned n=0;n<20;++n)m[at+n]=0;store32(m,at,patch);m[at+14]=64;m[at+18]=uint8_t(unison);}
   for(unsigned n=0;n<256;++n)m[physical+n]=0;m[physical]=uint8_t(mode);m[physical+4]=64;m[physical+79]=0xa5;m[physical+147]=0x23;
   for(unsigned v=0;v<256;++v)for(unsigned n=0;n<20;++n)m[0x17560 + v*20+n]=0;
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  x.pitch=y.pitch=127;x.middle=y.middle=x.low=y.low=0;
  a->pc=a->makepc(0x8fcc);b->pc=b->makepc(0x8fcc);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" mode="<<mode<<" unison="<<unison<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count%97==5){shared.resetMetrics();++resets;}
   if(mutate&&a->getPC()>=0x8fcc&&a->getPC()<=0x9028){a->regs[5].rl=b->regs[5].rl=int8_t(note+count);a->regs[5].rh=b->regs[5].rh=int8_t(velocity+count*7);
    for(auto* m:{a->memory,b->memory}){m[0x17526]=uint8_t((part+count)%4);m[physical]=uint8_t(mode^count);}}
   step();check(++count<100000,"Unison startup stalled");
   if(nested&&count==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-512;
    a->setSP(child);b->setSP(child);a->pc=a->makepc(0x8fcc);b->pc=b->makepc(0x8fcc);unsigned childCount=0;
    do{step();check(++childCount<100000,"Nested unison stalled");}while(a->getPC()!=0x902a||a->getSP()!=child);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=0x902a||a->getSP()!=stack);
  check(!b->interpretedSteps&&!shared.voiceActions.unisonCalls.partialEntries,"Unison used interpreter/recovery");
  check(shared.voiceActions.unisonCalls.completed>=1,"Unison final single-voice tail not handed off");check(std::memcmp(a->memory+0x160d2,b->memory+0x160d2,0x40000-0x160d2)==0,"Unison data memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned mode=0;mode<256;++mode)for(unsigned unison:{0u,7u,0xf8u,0xffu})verify(mode,unison,mode,clockOnly,bool(mode&1),unsigned(-1));
  for(unsigned part=0;part<4;++part)for(unsigned flags=0;flags<256;++flags)verify(3,0xa5,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,part,flags,255-flags,bool(flags&2));
  for(unsigned mode:{0u,1u,7u,8u,15u,31u,255u}){
   first.fill(0);verify(mode,0xa5,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::UnisonCalls::candidate(pc))verify(mode,0xa5,0xe7,clockOnly,true,selected[pc]-1);
   verify(mode,0xa5,0xd0,clockOnly,true,unsigned(-1),true);
  }
  for(unsigned mode:{0u,1u,7u})verify(mode,0xa5,0xd0,clockOnly,true,unsigned(-1),false,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Unison final full memory differs");
 std::cout<<"PASS: "<<cases<<" native unison handoffs, "<<steps<<" independent original steps; all mode and flag bytes, part/input/upper-count edges, low/tagged patch bindings, both fetch modes and SRAM alignments, live input/part/mode reads; "<<irqCases<<" IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested jobs; exact data/clock/state/full memory; zero interpreter/recovery; single-voice child remains compatibility\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
