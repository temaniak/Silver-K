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
    check(a.getPC()==b.getPC(),"note-on-state call PC projection differs");
    check(a.cycles==b.cycles,"note-on-state call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"note-on-state call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("note-on-state call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"note-on-state call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"note-on-state call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original note-on-state fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original note-on-state fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original note-on-state fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("note-on-state call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Note-on state fixture linked recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b53524b);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x4000> first{};
 auto verify=[&](bool empty,unsigned part,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned note=60,unsigned velocity=100,unsigned down=0){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u,entry=0x2efc;
  a->regs[5].r=b->regs[5].r=int16_t((velocity<<8)|note);a->setSP(stack);b->setSP(stack);for(auto* m:{a->memory,b->memory}){
   for(unsigned p=0;p<256;++p)for(unsigned at=0;at<334;++at)m[0x160d2 + p*334+at]=uint8_t(p*37+at*7+flags);m[0x17526]=uint8_t(part);m[0x169f5]=255;const unsigned partBase=0x160d2 + part*334;m[partBase+306]=uint8_t(down);m[partBase+324]=0;m[partBase+327]=uint8_t(flags&0x18);if(empty)for(unsigned at=0;at<16;++at)m[partBase+0xc0+at]=0;
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" empty="<<empty<<" note="<<note<<" velocity="<<velocity<<" count="<<down<<" part="<<part<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count%7==5){shared.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){for(unsigned p=0;p<256;++p)for(unsigned at=0;at<334;++at)m[0x160d2 + p*334+at]=uint8_t(count*23+at*31+p);m[0x17526]=uint8_t(count*17);}
   step();check(++count<4096,"Native note-on state stalled");
   if(nested&&count==5){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->pc=a->makepc(0x2efc);b->pc=b->makepc(0x2efc);unsigned childCount=0;
    do{step();check(++childCount<4096,"Nested note-on state stalled");if(childCount==5){shared.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=0x4338);
  check(!b->interpretedSteps&&!shared.noteActions.noteOnStateCalls.partialEntries&&!shared.noteActions.partControlCalls.partialEntries&&!shared.noteActions.keyActivityCalls.partialEntries&&!shared.noteActions.keyResetCalls.partialEntries,"Native note-on state used recovery");
  check(shared.noteActions.noteOnStateCalls.completed>=1,"Native note-on state did not complete");check(std::memcmp(a->memory+0x160d2,b->memory+0x160d2,256*334)==0,"Part memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned note=0;note<256;++note)for(unsigned velocity:{0u,1u,63u,127u,128u,255u})for(unsigned down:{0u,1u,31u,32u,128u,255u})verify(false,2,0xd0,clockOnly,false,unsigned(-1),false,false,note,velocity,down);
  for(bool empty:{false,true}){
   for(unsigned part:{0u,1u,2u,3u,255u})for(unsigned flags=0;flags<256;++flags)for(bool odd:{false,true})verify(empty,part,flags,clockOnly,odd,unsigned(-1));
   first.fill(0);verify(empty,2,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::NoteOnStateCalls::candidate(pc))verify(empty,2,0xe7,clockOnly,true,selected[pc]-1);
   verify(empty,3,0xd0,clockOnly,true,unsigned(-1),true);
   for(unsigned part:{0u,3u,255u})verify(empty,part,0xd0,clockOnly,true,unsigned(-1),false,true);
  }
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual note-on state calls, "<<steps<<" independent original steps; all-byte notes, velocity0/1/63/127/128/255, count0/1/31/32/128/255, empty/occupied physical maps, all flags, part0/1/2/3/255, both fetch modes, odd/even SRAM, live part/memory mutation; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
