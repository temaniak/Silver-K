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
    check(a.getPC()==b.getPC(),"voice-selection call PC projection differs");
    check(a.cycles==b.cycles,"voice-selection call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"voice-selection call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("voice-selection call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"voice-selection call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"voice-selection call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original voice-selection fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original voice-selection fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original voice-selection fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("voice-selection call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Selection fixture linked synchronized H8/recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b535653);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0xa530> first{};
 auto store32=[](uint8_t* m,unsigned at,uint32_t value){for(unsigned i=0;i<4;++i)m[at+i]=uint8_t(value>>(24-8*i));};
 auto verify=[&](unsigned mode,unsigned held,unsigned shape,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,bool gateOnly=false,unsigned seed=0){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());a->regs[6].er=b->regs[6].er=0x30000;a->regs[5].er=b->regs[5].er=seed&1?0x600000:0x400000;
  const unsigned voices=shape==8?256:14,part=seed%4,unison=seed%8,stack=odd?0xfff001u:0xa18000u;
  a->setSP(stack);b->setSP(stack);for(auto* m:{a->memory,b->memory}){
   m[0x400004]=m[0x600004]=7;m[0x30000]=uint8_t((mode<<3)|(seed&0xe7));m[0x17526]=uint8_t(part);m[0x174ae + part*20+17]=uint8_t(held);m[0x174ae + part*20+18]=uint8_t(unison);m[0x174a6]=uint8_t(voices);m[0x17528]=60;m[0x174a7]=uint8_t(seed%voices);
   for(unsigned voice=0;voice<voices;++voice){const unsigned base=0x17560+voice*20;uint32_t age=shape==0?0:shape==1?0x80000000u+voice:shape==2?0x10000u+voice:shape==3?0x100u+voice:shape==4?0x1000000u+voices-voice:shape==5?0xffffffffu:shape==6?0x1000000u:shape==7?0xc0000000u+voice:shape==8?0x10000u+voice:uint32_t((voice*0x9e3779b9u)^(seed*0x85ebca6bu));store32(m,base+8,age);m[base+4]=uint8_t(shape==4?(part+1)%4:shape==6?(voice==voices-1?part:(part+1)%4):shape==9?(voice+seed)%4:part);m[base+12]=uint8_t(shape==9?(voice+seed)%8:unison);m[base+5]=uint8_t(shape==7&&voice==voices-1?60:shape==9?(voice+seed)%128:61);}
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  const unsigned entry=gateOnly?0x8fc0:0xa338,returned=gateOnly?0x4338:0xa524;a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" mode="<<mode<<" held="<<held<<" shape="<<shape<<" seed="<<seed<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count==5){shared.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){
    m[0x30000]=uint8_t(((mode+(count>>4))%4)<<3);m[0x174ae + part*20+17]=uint8_t(held+count);m[0x174ae + part*20+18]=uint8_t(unison+count);m[0x17526]=uint8_t((part+(count>>3))%4);
    m[0x174a7]=uint8_t((seed+count)%voices);for(unsigned voice=0;voice<voices;++voice){const unsigned base=0x17560+voice*20;store32(m,base+8,uint32_t((voice+count)*0x9e3779b9u));m[base+4]=uint8_t((part+voice+count)%4);m[base+12]=uint8_t((unison+count+voice)%8);m[base+5]=uint8_t((voice+count)%128);}
   }
   step();check(++count<30000,"Native voice selection stalled");
   if(nested&&count==5){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->regs[6].er=b->regs[6].er=0x30000;a->pc=a->makepc(entry);b->pc=b->makepc(entry);unsigned childCount=0;
    do{step();check(++childCount<30000,"Nested voice selection stalled");if(childCount==5){shared.resetMetrics();++resets;}}while(a->getPC()!=returned);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=returned);
  check(!b->interpretedSteps&&!shared.voiceActions.selectionCalls.partialEntries&&!shared.noteActions.partControlCalls.partialEntries&&!shared.voiceActions.routingCalls.partialEntries&&!shared.voiceActions.selectCalls.partialEntries&&!shared.voiceActions.addressCalls.partialEntries&&!shared.voiceActions.wordCalls.partialEntries,"Native selection used recovery/interpreter");
  check(gateOnly?shared.voiceActions.selectionCalls.gateCompleted>=1:shared.voiceActions.selectionCalls.completed>=1,"Native selection/gate did not complete lifetime");++cases;if(injected)++irqCases;return count;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned mode=0;mode<4;++mode)for(unsigned held:{0u,1u,128u,255u})for(unsigned shape=0;shape<10;++shape)for(unsigned flags=0;flags<256;++flags)verify(mode,held,shape,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,false,flags);
  for(unsigned seed=0;seed<4096;++seed)verify(seed%4,seed%5?seed&255:0,9,seed&255,clockOnly,bool(seed&1),unsigned(-1),false,false,false,seed);
  for(unsigned mode=0;mode<4;++mode)for(unsigned held:{0u,1u})for(unsigned shape:{0u,1u,2u,4u,6u,7u}){
   first.fill(0);verify(mode,held,shape,0xd0,clockOnly,false,unsigned(-1),false,false,false,2);const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::VoiceSelectionCalls::candidate(pc))verify(mode,held,shape,0xe7,clockOnly,true,selected[pc]-1,false,false,false,2);
   verify(mode,held,shape,0xd0,clockOnly,true,unsigned(-1),true,false,false,2);
  }
  for(unsigned mode=0;mode<4;++mode)verify(mode,1,2,0xd0,clockOnly,true,unsigned(-1),false,true,false,2);
  for(unsigned flags=0;flags<256;++flags)verify(0,0,2,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,true,flags);
  first.fill(0);const unsigned gateSteps=verify(0,0,2,0xd0,clockOnly,false,unsigned(-1),false,false,true,2);
  for(unsigned at=0;at<gateSteps;++at)verify(0,0,2,0xe7,clockOnly,true,at,false,false,true,2);
  verify(0,0,2,0xd0,clockOnly,true,unsigned(-1),false,true,true,2);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Selection final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual native selection/gate calls, "<<steps<<" independent original steps; all modes/held kinds/age shapes/flags, random owner/age/note states,14/256 slots, both DSPs, mapped/clock-only fetch and odd/even SRAM, live state mutation; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
