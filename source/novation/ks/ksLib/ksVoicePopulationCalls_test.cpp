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
    check(a.getPC()==b.getPC(),"voice-population call PC projection differs");
    check(a.cycles==b.cycles,"voice-population call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"voice-population call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("voice-population call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"voice-population call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"voice-population call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original voice-population fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original voice-population fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original voice-population fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("voice-population call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Population fixture linked synchronized H8/recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b535650);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0xa600> first{};
 auto verify=[&](unsigned note,unsigned transpose,unsigned velocity,unsigned curve,unsigned power,unsigned pitch,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned part=2,unsigned slot=13,bool tagged=false){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const uint32_t patch=tagged?0xffa30000u:0x30000u;a->regs[6].er=b->regs[6].er=int32_t(patch);const unsigned physical=patch&0xffffff,stack=odd?0xfff001u:0xa18000u;
  a->setSP(stack);b->setSP(stack);for(auto* m:{a->memory,b->memory}){
   m[0x400004]=m[0x600004]=7;m[0x17526]=uint8_t(part);m[0x174a7]=uint8_t(slot);m[0x17528]=uint8_t(note);m[0x17529]=uint8_t(velocity);
   for(unsigned p=0;p<4;++p)for(unsigned at=0;at<20;++at)m[0x174ae + p*20+at]=uint8_t(p*37+at*7);
   m[0x174ae + part*20+14]=uint8_t(transpose);m[0x174ae + part*20+16]=uint8_t(curve);m[0x174ae + part*20+18]=3;m[0x169f1]=uint8_t(power);m[physical+173]=0xa5;
   for(unsigned at=0;at<252;++at)m[0x1534e + at]=uint8_t(at*7+31);m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  x.pitch=y.pitch=uint8_t(pitch);x.middle=y.middle=0x55;x.low=y.low=0xaa;
  a->pc=a->makepc(0xa524);b->pc=b->makepc(0xa524);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" note="<<note<<" transpose="<<transpose<<" velocity="<<velocity<<" curve="<<curve<<" power="<<power<<" pitch="<<pitch<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count==5){shared.resetMetrics();++resets;}
   if(mutate){x.pitch=y.pitch=uint8_t(pitch+count*13);x.middle=y.middle=uint8_t(count*17);x.low=y.low=uint8_t(count*19);
    for(auto* m:{a->memory,b->memory}){m[0x17526]=uint8_t((part+(count>>3))%4);m[0x174a7]=uint8_t((slot+count)%14);m[0x17528]=uint8_t(note+count*11);m[0x17529]=uint8_t(velocity+count*23);m[0x169f1]=uint8_t(power+count*29);m[physical+173]=uint8_t(count*31);
     for(unsigned p=0;p<4;++p){m[0x174ae + p*20+14]=uint8_t(transpose+count*37);m[0x174ae + p*20+16]=uint8_t(curve+count*41);m[0x174ae + p*20+18]=uint8_t(count*43);}
     for(unsigned v=0;v<256;++v)m[0x17560 + v*20+13]=uint8_t(note+count*47+v);
    }
   }
   step();check(++count<4096,"Native population stalled");
   if(nested&&count==5){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->regs[6].er=b->regs[6].er=int32_t(patch);a->pc=a->makepc(0xa524);b->pc=b->makepc(0xa524);unsigned childCount=0;
    do{step();check(++childCount<4096,"Nested population stalled");if(childCount==5){shared.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=0x4338);
  check(!b->interpretedSteps&&!shared.voiceActions.populationCalls.partialEntries&&!shared.noteActions.partControlCalls.partialEntries&&!shared.voiceActions.partControlCalls.partialEntries&&!shared.voiceActions.velocityCurveCalls.partialEntries&&!shared.voiceActions.routingCalls.partialEntries&&!shared.voiceActions.selectCalls.partialEntries&&!shared.voiceActions.addressCalls.partialEntries,"Native population used recovery/interpreter");
  check(shared.voiceActions.populationCalls.completed>=1,"Native population did not complete lifetime");++cases;if(injected)++irqCases;return count;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned note=0;note<256;++note)for(unsigned transpose=0;transpose<256;++transpose)verify(note,transpose,100,3,0,127,(note+transpose*149)&255,clockOnly,bool(note&1),unsigned(-1));
  for(unsigned note=0;note<256;++note)for(unsigned pitch=0;pitch<256;++pitch)verify(note,64,100,0,0,pitch,(note*149+pitch)&255,clockOnly,bool(pitch&1),unsigned(-1));
  for(unsigned velocity:{0u,1u,127u,128u,255u})for(unsigned curve:{0u,1u,2u,3u,4u,5u,255u})for(unsigned power=0;power<256;++power)verify(60,64,velocity,curve,power,127,(velocity+curve+power)&255,clockOnly,bool(power&1),unsigned(-1));
  for(unsigned part=0;part<4;++part)for(unsigned slot:{0u,13u,255u})for(bool tagged:{false,true})for(unsigned flags=0;flags<256;++flags)verify(60,64,100,7,0,127,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,part,slot,tagged);
  for(unsigned curve:{0u,1u,2u,3u,4u,5u,255u})for(unsigned pitch:{0u,127u,255u}){
   first.fill(0);verify(60,64,100,curve,0,pitch,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::VoicePopulationCalls::candidate(pc))verify(60,64,100,curve,0,pitch,0xe7,clockOnly,true,selected[pc]-1);
   verify(60,64,100,curve,0,pitch,0xd0,clockOnly,true,unsigned(-1),true);
  }
  for(unsigned power:{0u,126u,127u,128u,254u,255u})verify(60,64,255,5,power,127,0xd0,clockOnly,true,unsigned(-1),false,true,3,255,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Population final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual native population calls, "<<steps<<" independent original steps; all note/transpose and note/pitch pairs, power/curve/velocity edges, all flags/parts and slot0/13/255, low/tagged patch bindings, both fetch modes, odd/even SRAM, live fields/reply mutation; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
