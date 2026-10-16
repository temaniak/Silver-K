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
    check(a.getPC()==b.getPC(),"scheduled-timer call PC projection differs");
    check(a.cycles==b.cycles,"scheduled-timer call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"scheduled-timer call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("scheduled-timer call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"scheduled-timer call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"scheduled-timer call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original scheduled-timer fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original scheduled-timer fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original scheduled-timer fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("scheduled-timer call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Scheduled timer fixture linked recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b53524b);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x4200> first{};
 auto verify=[&](unsigned kind,unsigned slot,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned interval=127,unsigned captured=0xfff0,unsigned now=0x70){
  shared.scheduledTimerCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u,entry=0x4016+0x5c*slot+(kind==0?0:kind==1?10:36);
  a->regs[5].er=b->regs[5].er=int32_t(0x5b004000);a->regs[6].r=b->regs[6].r=int16_t(interval);a->setSP(stack);b->setSP(stack);for(auto* m:{a->memory,b->memory}){
   const unsigned at=0x16758+8*slot;m[at]=uint8_t(captured>>8);m[at+1]=uint8_t(captured);m[at+2]=uint8_t(interval>>8);m[at+3]=uint8_t(interval);m[at+4]=0x5b;m[at+5]=0;m[at+6]=0x40;m[at+7]=0;m[0xffec10]=0x12;m[0xffec11]=0x34;m[0xffec12]=uint8_t(now>>8);m[0xffec13]=uint8_t(now);m[0x17526]=2;
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" kind="<<kind<<" slot="<<slot<<" interval="<<interval<<" captured="<<captured<<" now="<<now<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count%7==5){shared.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){const unsigned at=0x16758+8*slot;m[at]=uint8_t(count*23);m[at+1]=uint8_t(count*31);m[at+2]=uint8_t(count*7);m[at+3]=uint8_t(count*17);m[0xffec12]=uint8_t(count*3);m[0xffec13]=uint8_t(count*71);}
   step();check(++count<256,"Native scheduled timer stalled");
   if(nested&&count==5){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->pc=a->makepc(0x4016+0x5c*((slot+1)%5)+(kind==0?10:0));b->pc=b->makepc(0x4016+0x5c*((slot+1)%5)+(kind==0?10:0));unsigned childCount=0;
    do{step();check(++childCount<256,"Nested scheduled timer stalled");if(childCount==5){shared.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=0x4338);
  check(!b->interpretedSteps&&!shared.scheduledTimerCalls.partialEntries&&!shared.noteActions.partControlCalls.partialEntries,"Native scheduled timer used recovery");
  check(shared.scheduledTimerCalls.completed>=1,"Native scheduled timer did not complete");check(std::memcmp(a->memory+0x16758,b->memory+0x16758,40)==0,"Part memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true})for(unsigned kind:{0u,1u,2u})for(unsigned slot=0;slot<5;++slot){
  for(unsigned flags=0;flags<256;++flags)for(bool odd:{false,true})verify(kind,slot,flags,clockOnly,odd,unsigned(-1));
  for(unsigned interval:{0u,1u,127u,128u,32767u,32768u,65535u})for(unsigned captured:{0u,1u,0x7fffu,0x8000u,0xfff0u,0xffffu})for(unsigned now:{0u,1u,0x70u,0x7fffu,0x8000u,0xffffu})verify(kind,slot,0xd0,clockOnly,false,unsigned(-1),false,false,interval,captured,now);
  first.fill(0);verify(kind,slot,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
  for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::ScheduledTimerCalls::candidate(pc))verify(kind,slot,0xe7,clockOnly,true,selected[pc]-1);
  verify(kind,slot,0xd0,clockOnly,true,unsigned(-1),true);
  verify(kind,slot,0xd0,clockOnly,true,unsigned(-1),false,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual scheduled timer calls, "<<steps<<" independent original steps; cancel/arm/service, all five slots/all flags, interval/captured/clock zero/sign/wrap edges, live timer mutation, callback handles, both fetch modes, odd/even SRAM; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
