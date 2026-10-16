#include "ksRuntimeController.h"
#include "ksKeyPriorityCalls.h"
#include "ksNoteOffStateCalls.h"


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
 std::mt19937 rng(0x4b53524b);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x3180> first{};
 auto verify=[&](unsigned part,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,unsigned note,unsigned pedal,unsigned mode,unsigned shape,bool mutate=false,bool nested=false){
  shared.noteActions.noteOffStateCalls.discardCalls();shared.noteActions.keyPriorityCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();shared.noteActions.noteOffStateCalls.resetMetrics();shared.noteActions.keyPriorityCalls.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u,entry=0x2f92;
  a->setSP(stack);b->setSP(stack);a->regs[5].rl=b->regs[5].rl=int8_t(note);
  for(auto* m:{a->memory,b->memory}){
   for(unsigned p=0;p<256;++p){const unsigned base=0x160d2 + p*334;for(unsigned at=0;at<334;++at)m[base+at]=uint8_t(p*37+at*7+flags);
    for(unsigned at=192;at<240;++at)m[base+at]=0;
    m[base+240]=117;m[base+241]=uint8_t(note);m[base+307]=m[base+306]=1;m[base+327]=uint8_t(pedal);m[base+324]=uint8_t(mode);
    m[base+192+note/8]=255;if(shape)m[base+208+note/8]=uint8_t(1u<<(note&7));
    if(shape==2)m[base+208+((note+17)&127)/8]|=uint8_t(1u<<((note+17)&7));if(shape==3)for(unsigned at=208;at<224;++at)m[base+at]=uint8_t(at*37+flags);
   }
   for(unsigned slot=0;slot<256;++slot){const unsigned at=0x17560+slot*20+8;m[at]=m[at+1]=m[at+2]=m[at+3]=0;}
   m[0x17528]=14;m[0x17526]=uint8_t(part);m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned countSteps=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" part="<<part<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<" note="<<note<<" pedal="<<pedal<<" mode="<<mode<<" shape="<<shape<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=countSteps+1;++steps;};
  do{
   if(countSteps==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(countSteps%7==5){shared.resetMetrics();shared.noteActions.noteOffStateCalls.resetMetrics();shared.noteActions.keyPriorityCalls.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){for(unsigned p=0;p<256;++p){const unsigned base=0x160d2 + p*334;for(unsigned at=0;at<334;++at)m[base+at]=uint8_t(countSteps*23+at*31+p);for(unsigned at=224;at<240;++at)m[base+at]=0;}
    for(unsigned slot=0;slot<256;++slot){const unsigned at=0x17560+slot*20+8;m[at]=m[at+1]=m[at+2]=m[at+3]=0;}m[0x17528]=1;m[0x17526]=uint8_t(countSteps*17);}
   step();check(++countSteps<50000,"Native Note Off stalled");
   if(nested&&countSteps==12){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->pc=a->makepc(0x2f92);b->pc=b->makepc(0x2f92);unsigned childCount=0;
    do{step();check(++childCount<50000,"Nested Note Off stalled");if(childCount==5){shared.resetMetrics();shared.noteActions.noteOffStateCalls.resetMetrics();shared.noteActions.keyPriorityCalls.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=0x4338);
  check(!b->interpretedSteps&&!shared.noteActions.noteOffStateCalls.partialEntries&&!shared.noteActions.keyPriorityCalls.partialEntries&&!shared.noteActions.keySearchCalls.partialEntries&&!shared.noteActions.partControlCalls.partialEntries,"Native Note Off used recovery");
  check(shared.noteActions.noteOffStateCalls.completed>=1,"Native Note Off did not complete");check(std::memcmp(a->memory+0x160d2,b->memory+0x160d2,256*334+850)==0,"Part memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned note=0;note<256;++note)for(unsigned shape=0;shape<4;++shape)verify((note+shape)%4,note,clockOnly,note&1,unsigned(-1),note,(note%3==0)?16:0,64,shape);
  for(unsigned part:{0u,1u,2u,3u,255u})for(unsigned flags=0;flags<256;++flags)verify(part,flags,clockOnly,flags&1,unsigned(-1),60,flags,64,flags%4);
  for(unsigned mode=0;mode<256;++mode)for(unsigned pedal:{0u,16u,32u,64u})verify(2,0xd0,clockOnly,mode&1,unsigned(-1),60,pedal,mode,1);
  for(unsigned shape=0;shape<4;++shape)for(unsigned pedal:{0u,16u}){first.fill(0);verify(2,0xd0,clockOnly,false,unsigned(-1),60,pedal,64,shape);const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::NoteOffStateCalls::candidate(pc))verify(2,0xe7,clockOnly,true,selected[pc]-1,60,pedal,64,shape);
  }
  verify(3,0xd0,clockOnly,true,unsigned(-1),60,0,64,1,true);
  for(unsigned part:{0u,3u,255u})verify(part,0xd0,clockOnly,true,unsigned(-1),60,0,64,1,false,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual Note Off calls, "<<steps<<" independent original steps; full-byte notes/pedal/mode/flags, sustain and held/nonheld mono branches, part0/1/2/3/255, both fetch modes, odd/even SRAM, live part/memory mutation; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
