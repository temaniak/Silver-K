#include "ksRuntimeController.h"
#include "ksParameterScalarCalls.h"
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
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);auto& calls=shared.voiceActions.parameterScalarCalls;OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b534f4d);uint64_t cases=0,steps=0,irqs=0,resets=0,nests=0;std::array<unsigned,0x9000> first{};
 struct Binding{unsigned entry,end,offset,dispatch;};constexpr Binding bindings[]{{0x7e20,0x7e3a,128,0x7e36},{0x7e3a,0x7e54,12,0x7e50},{0x7e54,0x7e70,14,0x7e6c},{0x7e70,0x7e8c,13,0x7e88},{0x8018,0x8032,31,0x802e},{0x8032,0x804c,32,0x8048},{0x804c,0x8066,33,0x8062},{0x8094,0x80ae,129,0x80aa},{0x80ae,0x80c8,20,0x80c4},{0x80c8,0x80e4,22,0x80e0},{0x80e4,0x8100,21,0x80fc},{0x828c,0x82a6,31,0x82a2},{0x82a6,0x82c0,32,0x82bc},{0x82c0,0x82da,33,0x82d6},{0x8308,0x8322,130,0x831e},{0x8322,0x833c,28,0x8338},{0x833c,0x8358,30,0x8354},{0x8358,0x8374,29,0x8370},{0x8500,0x851a,31,0x8516},{0x851a,0x8534,32,0x8530},{0x8534,0x854e,33,0x854a},{0x857c,0x8592,42,0x858e},{0x8592,0x85a8,146,0x85a4},{0x85d2,0x85e8,39,0x85e4},{0x85f2,0x8608,143,0x8604},{0x8612,0x8628,38,0x8624},{0x8632,0x8648,142,0x8644},{0x8652,0x8668,37,0x8664},{0x8672,0x8688,141,0x8684},{0x8692,0x86a8,171,0x86a4},{0x86a8,0x86be,40,0x86ba},{0x86be,0x86d4,144,0x86d0},{0x86d4,0x86ea,41,0x86e6},{0x86ea,0x8700,145,0x86fc},{0x8700,0x8712,43,0x870e},{0x884a,0x8864,48,0x8860},{0x8864,0x887e,49,0x887a},{0x887e,0x8898,50,0x8894},{0x88d6,0x88ea,56,0x88e6},{0x8ab0,0x8ac4,64,0x8ac0}};
 auto verify=[&](unsigned binding,unsigned seed,unsigned flags,bool clockOnly,bool odd,bool tagged,bool mutate,unsigned irqAt,bool nested=false,bool direct=false){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();calls.discardCalls();shared.resetMetrics();calls.resetMetrics();
  for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const auto bind=bindings[binding];const unsigned entry=direct?bind.dispatch:bind.entry;
  const unsigned stack=odd?0xfff001u:0xa18000u;const uint32_t patch=tagged?0xffa30000u:0x30000u;const unsigned physical=patch&0xffffff;
  a->regs[6].er=b->regs[6].er=int32_t(patch);a->regs[5].er=b->regs[5].er=int32_t(seed&1?0x400000:0x600000);
  a->setSP(stack);b->setSP(stack);std::mt19937 data(seed);
  for(unsigned i=0;i<180;++i)a->memory[physical+i]=b->memory[physical+i]=uint8_t(data());
  for(unsigned i=0;i<130;++i)a->memory[0x15200+i]=b->memory[0x15200+i]=uint8_t(data());
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
   if(mutate)for(unsigned i=0;i<130;++i)a->memory[0x15200+i]=b->memory[0x15200+i]=uint8_t(seed+count*47+i*17);
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
 for(bool clockOnly:{false,true})for(unsigned binding=0;binding<40;++binding){
  for(unsigned seed=0;seed<256;++seed)for(unsigned flags:{0u,1u,2u,4u,8u,0x20u,0x80u,0xffu})verify(binding,seed,flags,clockOnly,bool(seed&1),bool(seed&2),false,unsigned(-1));
  for(unsigned flags=0;flags<256;++flags)verify(binding,flags,flags,clockOnly,bool(flags&1),bool(flags&2),true,unsigned(-1));
  for(unsigned seed:{0u,64u,127u,128u,255u}){
   first.fill(0);verify(binding,seed,0xd0,clockOnly,false,false,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&pc>=bindings[binding].entry&&pc<bindings[binding].end)verify(binding,seed,0xe7,clockOnly,true,true,false,selected[pc]-1);
  }
  for(unsigned seed=0;seed<256;++seed)verify(binding,seed,seed,clockOnly,bool(seed&1),bool(seed&2),false,unsigned(-1),false,true);
  if(binding==3||binding==10||binding==17)for(unsigned seed:{0u,127u,255u})verify(binding,seed,0x80,clockOnly,true,true,false,unsigned(-1),true);
 }

 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final full memory differs");
 std::cout<<"PASS: "<<cases<<" complete scalar/pending calls, "<<steps<<" independent original steps; "<<irqs<<" IRQ admissions, "<<resets<<" resets, "<<nests<<" nested calls; random live fields/tables, flags/alignments/tags/both fetch modes; exact bus/clocks/state/full memory; zero recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
