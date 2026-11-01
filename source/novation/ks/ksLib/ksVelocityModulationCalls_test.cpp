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
    check(a.getPC()==b.getPC(),"velocity-modulation call PC projection differs");
    check(a.cycles==b.cycles,"velocity-modulation call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"velocity-modulation call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("velocity-modulation call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"velocity-modulation call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"velocity-modulation call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original velocity-modulation fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original velocity-modulation fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original velocity-modulation fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("velocity-modulation call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Modulation fixture linked synchronized recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b53564d);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x9200> first{};
 struct Binding {unsigned start,end,depth;};const Binding bindings[]={{0x8932,0x896a,58},{0x89f4,0x8a2c,61},{0x8b0e,0x8b46,66},{0x8d06,0x8d3e,169},{0x8e64,0x8e9c,170},{0x9084,0x90c0,58},{0x90c0,0x90fc,61},{0x90fc,0x9138,66}};
 auto verify=[&](unsigned binding,unsigned velocity,unsigned depth,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned slot=13,bool tagged=false){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u;const uint32_t patch=tagged?0xffa30000u:0x30000u;const unsigned physical=patch&0xffffff;
  a->regs[6].er=b->regs[6].er=int32_t(patch);a->regs[5].er=b->regs[5].er=int32_t(depth&1?0x400000:0x600000);a->regs[4].r=b->regs[4].r=int16_t(velocity*257u);
  a->setSP(stack);b->setSP(stack);for(auto* m:{a->memory,b->memory}){
   m[0x400004]=m[0x600004]=7;m[0x174a7]=uint8_t(slot);m[0x17526]=2;m[0x17560 + slot*20+6]=uint8_t(velocity);
   for(unsigned n=0;n<256;++n)m[physical+n]=uint8_t(n*7+velocity);m[physical+bindings[binding].depth]=uint8_t(depth);
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(bindings[binding].start);b->pc=b->makepc(bindings[binding].start);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;uint64_t completionFloor=0;bool injected=false,returned=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<" entry="<<bindings[binding].start<<std::dec<<" velocity="<<velocity<<" depth="<<depth<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   const bool last=returned;
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count%97==5&&!returned){shared.resetMetrics();++resets;}
   if(mutate){for(auto* m:{a->memory,b->memory}){m[0x174a7]=uint8_t((slot+count)%256);for(unsigned v=0;v<256;++v)m[0x17560 + v*20+6]=uint8_t(velocity+count*17+v);m[physical+bindings[binding].depth]=uint8_t(depth+count*23);}}
   step();check(++count<2048,"Native modulation stalled");if(last&&shared.voiceActions.modulationCalls.completed>completionFloor)break;
   if(nested&&count==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);a->pc=a->makepc(bindings[0].start);b->pc=b->makepc(bindings[0].start);unsigned childCount=0;uint64_t childFloor=0;bool childReturned=false;
    do{const bool final=childReturned;step();check(++childCount<2048,"Nested modulation stalled");if(final&&shared.voiceActions.modulationCalls.completed>childFloor)break;if(!childReturned&&a->getPC()==bindings[0].end&&a->getSP()==child){childReturned=true;childFloor=shared.voiceActions.modulationCalls.completed;}
     if(childCount==5&&!childReturned){shared.resetMetrics();++resets;}}while(true);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
   if(!returned&&a->getPC()==bindings[binding].end&&a->getSP()==stack){returned=true;completionFloor=shared.voiceActions.modulationCalls.completed;}
  }while(true);
  check(!b->interpretedSteps&&!shared.voiceActions.modulationCalls.partialEntries&&!shared.noteActions.partControlCalls.partialEntries&&!shared.voiceActions.wordCalls.partialEntries&&!shared.voiceActions.routingCalls.partialEntries,"Modulation used interpreter/recovery");
  const auto& calls=shared.voiceActions.modulationCalls;if(!calls.completed)std::cerr<<"completion binding="<<binding<<" velocity="<<velocity<<" depth="<<depth<<" flags="<<flags<<" count="<<count<<" irqAt="<<interruptAt<<" active="<<calls.activeCalls()<<" initial="<<calls.initialActive<<" started="<<calls.started<<" pc="<<std::hex<<a->getPC()<<std::dec<<'\n';check(calls.completed>=1,"Inline modulation completion was not observed");check(calls.started+calls.initialActive==calls.completed+calls.activeCalls(),"Modulation lifetime accounting differs");
  check(std::memcmp(a->memory+0x159f0,b->memory+0x159f0,0x19000-0x159f0)==0,"Modulation data memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true})for(unsigned binding=0;binding<8;++binding){
  for(unsigned velocity=0;velocity<256;++velocity)for(unsigned depth=0;depth<256;++depth)verify(binding,velocity,depth,(velocity+depth*149)&255,clockOnly,bool(velocity&1),unsigned(-1));
  for(unsigned slot:{0u,13u,255u})for(bool tagged:{false,true})for(unsigned flags=0;flags<256;++flags)verify(binding,255,192,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,slot,tagged);
  for(unsigned velocity:{0u,127u,128u,255u})for(unsigned depth:{0u,64u,192u,255u}){
   first.fill(0);verify(binding,velocity,depth,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::VelocityModulationCalls::candidate(pc))verify(binding,velocity,depth,0xe7,clockOnly,true,selected[pc]-1);
   verify(binding,velocity,depth,0xd0,clockOnly,true,unsigned(-1),true);
  }
  for(unsigned depth:{0u,64u,192u,255u})verify(binding,128,depth,0xd0,clockOnly,true,unsigned(-1),false,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Modulation final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual inline modulation jobs/caller admissions, "<<steps<<" independent original steps; every velocity/depth pair in all eight bindings, all flags/slot boundaries, low/tagged patches, both fetch modes and SRAM alignments, live fields; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested jobs; exact data/clocks/state/full memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
