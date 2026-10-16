#include "ksRuntimeController.h"
#include "ksKeyPriorityCalls.h"
#include "ksNoteOffStateCalls.h"
#include "ksPartNoteCalls.h"
#include "ksPedalCalls.h"
#include "ksMidiControlCalls.h"
#include "ksMidiDecodeCalls.h"






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
    check(a.getPC()==b.getPC(),"key-priority call PC projection differs");
    check(a.cycles==b.cycles,"key-priority call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"key-priority call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("key-priority call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"key-priority call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"key-priority call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original key-priority fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original key-priority fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original key-priority fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("key-priority call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Key priority fixture linked recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b53524b);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x2efc> first{};
 uint64_t seams=0;
 auto verify=[&](bool descriptor,unsigned byte,unsigned running,unsigned remaining,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,unsigned pointerKind,unsigned channel,bool mutate=false,bool nested=false){
  shared.midiDecodeCalls.discardCalls();shared.midiControlCalls.discardCalls();shared.noteActions.pedalNativeCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();shared.midiDecodeCalls.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u,entry=descriptor?0x2ed4:0x2de8;
  auto desc=[](unsigned status){return status>=0xf0?0xf27a+(status&15)*6:0xf250+((status&0x70)>>4)*6;};
  auto word=[](const uint8_t* m,unsigned at){return uint32_t(m[at])<<24|uint32_t(m[at+1])<<16|uint32_t(m[at+2])<<8|m[at+3];};
  unsigned stop=0x4338;bool seam=false;
  if(!descriptor){if(byte>=128){const auto at=desc(byte);if((a->memory[at+5]&2)||!a->memory[at+4])stop=word(a->memory,at)&0xffffff;}else if(remaining==1){stop=running>=0xf0?0x328e:0x588;seam=running>=0xf0;}}
  a->setSP(stack);b->setSP(stack);a->regs[0].rl=b->regs[0].rl=int8_t(byte);a->regs[6].rl=b->regs[6].rl=int8_t(byte);
  const uint32_t pointer=pointerKind==0?0x15c1f:pointerKind==1?0xa30000:pointerKind==2?0xa30001:0xffa30001u;
  for(auto* m:{a->memory,b->memory}){
   std::fill(m+0x15c00,m+0x40000,0);
   m[0x15c1b]=uint8_t(running);m[0x15c1c]=uint8_t(running);m[0x15c1d]=uint8_t(remaining);m[0x15c1e]=0xd7;m[0x15c1a]=uint8_t(channel);
   for(unsigned i=0;i<4;++i)m[0x15c16+i]=uint8_t(pointer>>(24-i*8));
   m[0x167d3]=0;m[0x167ba]=0;m[0x174a6]=1;m[0x400004]=m[0x600004]=7;
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned countSteps=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" descriptor="<<descriptor<<" byte="<<byte<<" running="<<running<<" remaining="<<remaining<<" pointer="<<pointerKind<<" flags="<<flags<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=countSteps+1;++steps;};
  do{
   if(countSteps==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(countSteps%7==5){shared.resetMetrics();shared.midiDecodeCalls.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){m[0x15c1a]=uint8_t(countSteps*17);if(byte<128&&remaining>1)m[0x15c1d]=uint8_t(countSteps*19);}
   step();check(++countSteps<10000,"Native MIDI decode stalled");
   if(nested&&countSteps==3){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->regs[0].rl=b->regs[0].rl=0xf8;a->pc=a->makepc(0x2ed4);b->pc=b->makepc(0x2ed4);unsigned childCount=0;
    do{step();check(++childCount<10000,"Nested MIDI descriptor stalled");if(childCount==5){shared.resetMetrics();shared.midiDecodeCalls.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=stop);
  check(!b->interpretedSteps&&!shared.midiDecodeCalls.partialEntries,"Native MIDI decode used recovery");
  if(!seam)check(descriptor?shared.midiDecodeCalls.descriptorCompleted>=1:stop==0x4338?shared.midiDecodeCalls.completed>=1:shared.midiDecodeCalls.handedOff>=1,"Native MIDI decode did not complete/handoff");
  check(std::memcmp(a->memory+0x15c00,b->memory+0x15c00,0x40000-0x15c00)==0,"MIDI decode memory differs");++cases;if(injected)++irqCases;if(seam)++seams;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned byte=0;byte<256;++byte)for(unsigned flags=0;flags<256;++flags)verify(true,byte,0x90,2,flags,clockOnly,flags&1,unsigned(-1),0,0);
  for(unsigned byte=0;byte<256;++byte)for(unsigned remaining:{0u,1u,2u,127u,128u,255u})for(unsigned pointerKind=0;pointerKind<4;++pointerKind)verify(false,byte,0x90,remaining,byte,clockOnly,pointerKind&1,unsigned(-1),pointerKind,byte);
  for(unsigned running=0;running<256;++running)verify(false,60,running,1,0xd0,clockOnly,running&1,unsigned(-1),0,running);
  for(unsigned flags=0;flags<256;++flags)verify(false,0x90,0xb0,2,flags,clockOnly,flags&1,unsigned(-1),0,7);
  for(unsigned shape=0;shape<8;++shape){const bool descriptor=shape==0;const unsigned byte=shape<3?0x90:shape==3?0xf0:shape==4?0xf8:60,remaining=shape==5?0:shape==6?2:1;
   first.fill(0);verify(descriptor,byte,0xb0,remaining,0xd0,clockOnly,false,unsigned(-1),0,7);const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::MidiDecodeCalls::candidate(pc))verify(descriptor,byte,0xb0,remaining,0xe7,clockOnly,true,selected[pc]-1,0,7);
  }
  verify(false,60,0x90,2,0xd0,clockOnly,true,unsigned(-1),1,7,true);
  for(bool descriptor:{false,true})verify(descriptor,0x90,0x90,2,0xd0,clockOnly,true,unsigned(-1),0,7,false,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual native MIDI byte/descriptor calls, "<<steps<<" independent original steps; full-byte status/data/remaining/channel/flags, channel/system descriptors, immediate/SysEx and running-status decisions, low/high/tagged odd/even payload pointers, both fetch modes and SRAM stacks/live mutations; "<<seams<<" system dispatch seams, "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
