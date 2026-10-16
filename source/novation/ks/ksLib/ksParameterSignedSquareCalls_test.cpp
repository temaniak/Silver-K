#include "ksRuntimeController.h"
#include "ksParameterSignedSquareCalls.h"
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
    check(a.getPC()==b.getPC(),"oscillator-modulation call PC projection differs");
    check(a.cycles==b.cycles,"oscillator-modulation call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"oscillator-modulation call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("oscillator-modulation call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"oscillator-modulation call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"oscillator-modulation call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original oscillator-modulation fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original oscillator-modulation fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original oscillator-modulation fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("oscillator-modulation call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP private OS\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Fixture linked synchronized recovery");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);auto& calls=shared.voiceActions.parameterSignedSquareCalls;OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b534f4d);uint64_t cases=0,steps=0,irqs=0,resets=0,nests=0;std::array<unsigned,0x9000> first{};
 struct Binding{unsigned entry,end,offset,dispatch;};constexpr Binding bindings[]{{0x88ea,0x890e,57,0x890a},{0x890e,0x8932,172,0x892e}};
 auto verify=[&](unsigned binding,unsigned seed,unsigned flags,bool clockOnly,bool odd,bool tagged,bool mutate,unsigned irqAt,bool nested=false,bool direct=false){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();calls.discardCalls();shared.resetMetrics();calls.resetMetrics();
  for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const auto bind=bindings[binding];const unsigned entry=direct?bind.dispatch:bind.entry;
  const unsigned stack=odd?0xfff001u:0xa18000u;const uint32_t patch=tagged?0xffa30000u:0x30000u;const unsigned physical=patch&0xffffff;
  a->regs[6].er=b->regs[6].er=int32_t(patch);a->regs[5].er=b->regs[5].er=int32_t(seed&1?0x400000:0x600000);
  a->setSP(stack);b->setSP(stack);std::mt19937 data(seed);
  for(unsigned i=0;i<180;++i)a->memory[physical+i]=b->memory[physical+i]=uint8_t(data());
  for(unsigned i=0;i<256;++i)a->memory[0x15200+i]=b->memory[0x15200+i]=uint8_t(data());
  a->memory[physical+bind.offset]=b->memory[physical+bind.offset]=uint8_t(seed);
  for(unsigned i=0;i<3;++i)a->memory[0x17518+i]=b->memory[0x17518+i]=uint8_t(data());
  for(auto* m:{a->memory,b->memory}){m[0x400004]=m[0x600004]=7;m[0x174a9]=0xff;m[0x174ab]=0xff;m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;}
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false,finished=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{
    runtime.step(*b);
    a->step();same(*a,*b,x,y,clockOnly);
   }catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" seed="<<seed<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<" tagged="<<tagged<<" mutate="<<mutate<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==irqAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count%61==5){calls.resetMetrics();shared.resetMetrics();++resets;}
   if(mutate)for(unsigned i=0;i<180;++i)a->memory[physical+i]=b->memory[physical+i]=uint8_t(seed+count*37+i*23);
   if(mutate)for(unsigned i=0;i<3;++i)a->memory[0x17518+i]=b->memory[0x17518+i]=uint8_t(seed+count*43+i*31);
   if(mutate)for(unsigned i=0;i<256;++i)a->memory[0x15200+i]=b->memory[0x15200+i]=uint8_t(seed+count*47+i*17);
   finished=a->getPC()==bind.end&&a->getSP()==stack;step();check(++count<2048,"Oscillator modulation stalled");
   if(nested&&count==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-512;
    a->setSP(child);b->setSP(child);a->pc=a->makepc(entry);b->pc=b->makepc(entry);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    unsigned n=0;bool innerFinished=false;do{innerFinished=a->getPC()==bind.end&&a->getSP()==child;step();check(++n<2048,"Nested oscillator modulation stalled");if(n==7){calls.resetMetrics();shared.resetMetrics();++resets;}}while(!innerFinished);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nests;
   }
  }while(!finished);
  check(!b->interpretedSteps&&!calls.partialEntries&&!shared.voiceActions.wordCalls.partialEntries&&!shared.voiceActions.routingCalls.partialEntries,"Oscillator modulation used recovery");
  check(calls.completed>=1,"Oscillator modulation job lifetime differs");check(calls.started+calls.initialActive==calls.completed+calls.activeCalls(),"Oscillator modulation metrics differ");
  check(std::memcmp(a->memory+0x15000,b->memory+0x15000,0x3000)==0,"Oscillator memory differs");++cases;if(injected)++irqs;
 };
 for(bool clockOnly:{false,true})for(unsigned binding=0;binding<2;++binding){
  for(unsigned seed=0;seed<256;++seed)for(unsigned flags:{0u,1u,2u,4u,8u,0x20u,0x80u,0xffu})verify(binding,seed,flags,clockOnly,bool(seed&1),bool(seed&2),false,unsigned(-1));
  for(unsigned flags=0;flags<256;++flags)verify(binding,flags,flags,clockOnly,bool(flags&1),bool(flags&2),true,unsigned(-1));
  for(unsigned seed:{0u,64u,127u,128u,255u}){
   first.fill(0);verify(binding,seed,0xd0,clockOnly,false,false,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&pc>=bindings[binding].entry&&pc<bindings[binding].end)verify(binding,seed,0xe7,clockOnly,true,true,false,selected[pc]-1);
  }

  if(binding==0)for(unsigned seed:{0u,127u,255u})verify(binding,seed,0x80,clockOnly,true,true,false,unsigned(-1),true);
 }

 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final full memory differs");
 std::cout<<"PASS: "<<cases<<" complete curve calls, "<<steps<<" independent original steps; "<<irqs<<" IRQ admissions, "<<resets<<" resets, "<<nests<<" nested calls; random live fields/tables, flags/alignments/tags/both fetch modes; exact bus/clocks/state/full memory; zero recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
