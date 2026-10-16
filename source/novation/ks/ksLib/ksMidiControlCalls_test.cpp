#include "ksRuntimeController.h"
#include "ksKeyPriorityCalls.h"
#include "ksNoteOffStateCalls.h"
#include "ksPartNoteCalls.h"
#include "ksPedalCalls.h"
#include "ksMidiControlCalls.h"





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
 std::mt19937 rng(0x4b53524b);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x352c> first{};
 auto verify=[&](unsigned kind,unsigned part,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,unsigned firstByte,unsigned secondByte,unsigned remap=64,bool nested=false){
  shared.midiControlCalls.discardCalls();shared.noteActions.pedalNativeCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();shared.midiControlCalls.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u,entry=kind==0?0x349c:kind==1?0x34b2:0x34bc;
  const unsigned cc=firstByte==11?remap:firstByte;
  const unsigned stop=kind<2?0x4338:cc==1?0x9e62:cc==2?0x9e82:cc==32||cc==120||cc==121||cc>=123&&cc<=127?0x4338:0x352c;
  a->setSP(stack);b->setSP(stack);a->regs[4].er=b->regs[4].er=0x31000;a->regs[5].er=b->regs[5].er=0x30000;
  for(auto* m:{a->memory,b->memory}){
   std::fill(m+0x15c00,m+0x40000,0);
   for(unsigned p=0;p<256;++p){const unsigned base=0x174ae + p*20;m[base]=0;m[base+1]=3;m[base+2]=m[base+3]=0;}
   std::fill(m+0x17560,m+0x17574,0);m[0x174a6]=1;m[0x400004]=m[0x600004]=7;m[0x17526]=uint8_t(part);m[0x30000+177]=uint8_t(remap);
   m[0x31000]=uint8_t(firstByte);m[0x31001]=uint8_t(secondByte);m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned countSteps=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" kind="<<kind<<" part="<<part<<" flags="<<flags<<" first="<<firstByte<<" second="<<secondByte<<" remap="<<remap<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=countSteps+1;++steps;};
  do{
   if(countSteps==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(countSteps%7==5){shared.resetMetrics();shared.midiControlCalls.resetMetrics();++resets;}
   step();check(++countSteps<50000,"Native MIDI control stalled");
   if(nested&&countSteps==3){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->pc=a->makepc(0x34b2);b->pc=b->makepc(0x34b2);unsigned childCount=0;
    do{step();check(++childCount<50000,"Nested MIDI control stalled");if(childCount==5){shared.resetMetrics();shared.midiControlCalls.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=stop);
  check(!b->interpretedSteps&&!shared.midiControlCalls.partialEntries&&!shared.noteActions.partControlCalls.partialEntries&&!shared.noteActions.releaseCalls.partialEntries,"Native MIDI control used recovery");
  check(stop==0x4338?shared.midiControlCalls.completed>=1:shared.midiControlCalls.handedOff>=1,"Native MIDI control did not complete/handoff");check(std::memcmp(a->memory+0x15c00,b->memory+0x15c00,0x40000-0x15c00)==0,"MIDI control memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned firstByte=0;firstByte<256;++firstByte)for(unsigned secondByte=0;secondByte<256;++secondByte)verify(0,(firstByte+secondByte)%4,firstByte,clockOnly,secondByte&1,unsigned(-1),firstByte,secondByte);
  for(unsigned kind:{1u,2u})for(unsigned firstByte=0;firstByte<256;++firstByte)for(unsigned secondByte:{0u,64u,127u,255u})verify(kind,firstByte%4,firstByte,clockOnly,secondByte&1,unsigned(-1),firstByte,secondByte);
  for(unsigned remap=0;remap<256;++remap)verify(2,2,0xd0,clockOnly,remap&1,unsigned(-1),11,0,remap);
  for(unsigned kind:{0u,1u,2u})for(unsigned part:{0u,1u,2u,3u,255u})for(unsigned flags=0;flags<256;++flags)verify(kind,part,flags,clockOnly,flags&1,unsigned(-1),kind==2?32:60,64);
  for(unsigned kind:{0u,1u,2u})for(unsigned shape=0;shape<(kind==2?10u:1u);++shape){const unsigned firstByte=kind==2?std::array<unsigned,10>{1,2,11,32,120,121,123,124,126,64}[shape]:60;
   first.fill(0);verify(kind,2,0xd0,clockOnly,false,unsigned(-1),firstByte,0,1);const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::MidiControlCalls::candidate(pc))verify(kind,2,0xe7,clockOnly,true,selected[pc]-1,firstByte,0,1);
  }
  for(unsigned kind:{0u,1u,2u})verify(kind,2,0xd0,clockOnly,true,unsigned(-1),kind==2?32:60,64,64,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual native pitch/pressure/CC calls, "<<steps<<" independent original steps; full-byte pitch pairs/controller/value/expression-remap/flags, bank/sound/reset/all-note/mono/poly/tail decisions, part0/1/2/3/255, both fetch modes, odd/even SRAM; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
