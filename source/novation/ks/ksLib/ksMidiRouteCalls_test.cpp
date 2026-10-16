#include "ksRuntimeController.h"
#include "ksKeyPriorityCalls.h"
#include "ksNoteOffStateCalls.h"
#include "ksPartNoteCalls.h"
#include "ksPedalCalls.h"
#include "ksMidiControlCalls.h"
#include "ksMidiAuxCalls.h"
#include "ksMidiRouteCalls.h"







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
 std::mt19937 rng(0x4b53524b);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x33b8> first{};
 auto verify=[&](unsigned handler,unsigned status,unsigned channel,unsigned mask,unsigned pattern,unsigned matches,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false){
  shared.midiRouteCalls.discardCalls();shared.midiAuxCalls.discardCalls();shared.midiControlCalls.discardCalls();shared.noteActions.pedalNativeCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();shared.midiRouteCalls.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u;const uint32_t reference=std::array<uint32_t,6>{0xa934,0x3600,0x34bc,0xff00a934u,0xff0034bcu,0xff003600u}[handler];
  const unsigned stop=status>=240?(reference&0xffffff):0x4338;
  a->setSP(stack);b->setSP(stack);a->regs[0].er=b->regs[0].er=int32_t(reference);a->regs[4].er=b->regs[4].er=0x31000;
  for(auto* m:{a->memory,b->memory}){
   std::fill(m+0x15c00,m+0x40000,0);
   for(unsigned p=0;p<256;++p)m[0x160d2+p*334+331]=255;
   for(unsigned p=0;p<256;++p){const unsigned base=0x174ae + p*20;m[base]=0;m[base+1]=3;m[base+2]=m[base+3]=0;m[base+11]=uint8_t(matches==0?channel:matches==1?channel+1:p%2?channel:channel+1);}
   std::fill(m+0x17560,m+0x17574,0);m[0x174a6]=1;m[0x400004]=m[0x600004]=7;m[0x17526]=17;m[0x15c22]=uint8_t(status);m[0x15c21]=uint8_t(channel);m[0x15c23]=0x5a;m[0x167d3]=uint8_t(mask);m[0x167ba]=uint8_t(pattern==0||pattern==2||pattern==6?0:pattern==5?255:1);
   m[0x31000]=32;m[0x31001]=64;m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(0x328e);b->pc=b->makepc(0x328e);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned countSteps=0,modeReads=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<" reference="<<reference<<std::dec<<" status="<<status<<" channel="<<channel<<" mask="<<mask<<" pattern="<<pattern<<" matches="<<matches<<" flags="<<flags<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=countSteps+1;++steps;};
  do{
   if(countSteps==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(countSteps%7==5){shared.resetMetrics();shared.midiRouteCalls.resetMetrics();++resets;}
   if(a->getPC()==0x331a){++modeReads;if(pattern==3&&modeReads>=1||pattern==4&&modeReads>=2)for(auto* m:{a->memory,b->memory})m[0x167ba]=0;}
   if(a->getPC()==0x3326&&(pattern==2||pattern==6||pattern==7))for(auto* m:{a->memory,b->memory})m[0x167ba]=uint8_t(pattern==7?0:1);
   if(mutate)for(auto* m:{a->memory,b->memory})m[0x17526]=uint8_t(countSteps*7);
   step();check(++countSteps<50000,"Native MIDI routing stalled");
   if(nested&&countSteps==6){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->regs[0].er=b->regs[0].er=0xa934;a->pc=a->makepc(0x328e);b->pc=b->makepc(0x328e);unsigned childCount=0;
    do{step();check(++childCount<50000,"Nested MIDI routing stalled");if(childCount==5){shared.resetMetrics();shared.midiRouteCalls.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=stop);
  check(!b->interpretedSteps&&!shared.midiRouteCalls.partialEntries&&!shared.noteActions.partControlCalls.partialEntries,"Native MIDI routing used recovery");
  check(status>=240?shared.midiRouteCalls.handedOff>=1:shared.midiRouteCalls.completed>=1,"Native MIDI routing did not complete/handoff");check(std::memcmp(a->memory+0x15c00,b->memory+0x15c00,0x40000-0x15c00)==0,"MIDI routing memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned handler:{0u,2u})for(unsigned mask=0;mask<256;++mask)for(unsigned pattern=0;pattern<8;++pattern)for(unsigned matches=0;matches<3;++matches)verify(handler,0xb0,7,mask,pattern,matches,mask,clockOnly,mask&1,unsigned(-1));
  for(unsigned channel=0;channel<256;++channel)for(unsigned handler=0;handler<6;++handler)verify(handler,0xb0,channel,handler==1||handler==5?0:15,1,0,channel,clockOnly,channel&1,unsigned(-1));
  for(unsigned status=0;status<256;++status)verify(0,status,7,15,0,0,0xd0,clockOnly,status&1,unsigned(-1));
  for(unsigned flags=0;flags<256;++flags)verify(2,0xb0,7,15,1,0,flags,clockOnly,flags&1,unsigned(-1));
  for(unsigned shape=0;shape<10;++shape){const unsigned handler=shape>=7?shape-6:shape==6?2:0,pattern=shape<7?shape:1,mask=handler==1?0:15;
   first.fill(0);verify(handler,0xb0,7,mask,pattern,0,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::MidiRouteCalls::candidate(pc))verify(handler,0xb0,7,mask,pattern,0,0xe7,clockOnly,true,selected[pc]-1);
  }
  verify(0,0xb0,7,15,1,0,0xd0,clockOnly,true,unsigned(-1),true);
  for(unsigned pattern:{0u,1u,3u})verify(0,0xb0,7,15,pattern,0,0xd0,clockOnly,true,unsigned(-1),false,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual native MIDI routing calls, "<<steps<<" independent original steps; per-part channel/mask/handler frames, global program/CC fallback mirror, mode exit unused-bank cleanup, raw tagged handler references/full-byte statuses/channels/masks/flags, both fetch modes, odd/even SRAM and live part/mode mutation; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
