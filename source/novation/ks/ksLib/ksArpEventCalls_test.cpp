#include "ksRuntimeController.h"
#include "ksKeyPriorityCalls.h"
#include "ksNoteOffStateCalls.h"
#include "ksPartNoteCalls.h"
#include "ksPedalCalls.h"
#include "ksMidiControlCalls.h"
#include "ksMidiAuxCalls.h"
#include "ksMidiRouteCalls.h"
#include "ksArpPrimitiveCalls.h"
#include "ksArpScheduleCalls.h"
#include "ksArpMainCalls.h"
#include "ksArpEventCalls.h"
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
 std::mt19937 rng(0x4b53524b);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x4000> first{};
 auto verify=[&](unsigned kind,unsigned value1,unsigned value2,unsigned policy,unsigned mode,unsigned shape,unsigned mask,unsigned phase,unsigned expiry,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,bool lifecycle=false){
  shared.arpEventCalls.discardCalls();shared.arpMainCalls.discardCalls();shared.arpScheduleCalls.discardCalls();shared.arpPrimitiveCalls.discardCalls();shared.midiRouteCalls.discardCalls();shared.midiAuxCalls.discardCalls();shared.midiControlCalls.discardCalls();shared.noteActions.pedalNativeCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();shared.arpEventCalls.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u,part=3,base=0x160d2 + part*334,entry=std::array<unsigned,7>{0x3ab2,0x3b30,0x3b68,0x3bc6,0x3bf0,0x3f18,0x3f80}[kind];
  a->setSP(stack);b->setSP(stack);a->regs[0].er=b->regs[0].er=int32_t(base);a->regs[5].r=b->regs[5].r=int16_t((kind==5?value2:192)<<8|(kind==5?value1:60));
  for(auto* m:{a->memory,b->memory}){std::fill(m+0x15c00,m+0x40000,0);
   for(unsigned p=0;p<5;++p){const unsigned pb=0x160d2 + p*334,ctl=0x174ae + p*20;m[ctl]=0;m[ctl+1]=3;m[ctl+2]=m[ctl+3]=0;m[ctl+12]=0;m[ctl+13]=127;m[pb+327]=uint8_t(policy);m[pb+324]=uint8_t(mode);m[pb+313]=uint8_t(shape%3);m[pb+330]=shape%3==0?127:64;m[pb+329]=uint8_t(mode);m[pb+319]=127;m[pb+326]=kind==5?uint8_t(shape):1;m[pb+321]=0;m[pb+325]=uint8_t(shape);m[pb+12]=0x12;m[pb+13]=0x34;
    for(unsigned i=0;i<256;++i)m[pb+16+i]=uint8_t(36+i%48);
    m[pb+312]=m[pb+320]=uint8_t(kind==4?value2:kind==5?0:value1);m[pb+311]=uint8_t(kind==5?0:value2);m[pb+304]=uint8_t(phase>>8);m[pb+305]=uint8_t(phase);m[pb+14]=uint8_t(expiry>>8);m[pb+15]=uint8_t(expiry);
    std::fill(m+pb+208,m+pb+224,0);if(shape&1)m[pb+208+7]=0x10;for(unsigned i=0;i<32;++i){m[pb+240+i*2]=192;m[pb+241+i*2]=uint8_t(36+i);}
    m[pb+307]=uint8_t(kind==4?value1:0);if(kind==6)m[pb+208]=uint8_t(value1);
   }
   m[0x30004]=64;m[0x3004f]=0;m[0x30058]=0;for(unsigned p=0;p<128;++p)m[92078+p*2]=m[92079+p*2]=0;
   std::fill(m+0x17560,m+0x17574,0);m[0x174a6]=1;m[0x174a7]=0;m[0x400004]=m[0x600004]=7;m[0x17528]=1;m[0x17526]=uint8_t(part);m[0x1752a]=m[0x1752b]=0;m[0x16798]=uint8_t(part);m[0x16790]=2;m[0x160d0]=uint8_t(mask);m[0xffed6c]=m[0xffed6d]=m[0xffed6e]=m[0xffed6f]=0;
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned countSteps=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" kind="<<kind<<" value1="<<value1<<" value2="<<value2<<" policy="<<policy<<" mode="<<mode<<" shape="<<shape<<" mask="<<mask<<" phase="<<phase<<" expiry="<<expiry<<" flags="<<flags<<'\n';throw;}if(before<first.size()&&!first[before])first[before]=countSteps+1;++steps;};
  do{
   if(countSteps==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(!lifecycle&&countSteps%13==5){shared.resetMetrics();shared.arpEventCalls.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){if(a->getPC()==0x3b6c||a->getPC()==0x3f84||a->getPC()==0x3fc6)m[0x17526]=uint8_t(countSteps%5);if(a->getPC()==0x3c90)m[base+324]=uint8_t(countSteps);}
   step();if(++countSteps>=500000){std::cerr<<"stalled="<<std::hex<<a->getPC()<<std::dec<<" kind="<<kind<<" value1="<<value1<<" value2="<<value2<<" policy="<<policy<<" mode="<<mode<<" shape="<<shape<<" mask="<<mask<<" flags="<<flags<<"\n";throw std::runtime_error("Native arp event stalled");}
   if(nested&&countSteps==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}a->regs[0].er=b->regs[0].er=int32_t(base);a->pc=a->makepc(0x3f80);b->pc=b->makepc(0x3f80);unsigned childCount=0;do{step();check(++childCount<500000,"Nested arp event stalled");if(childCount==5){shared.resetMetrics();shared.arpEventCalls.resetMetrics();++resets;}}while(a->getPC()!=0x4338);for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;}
  }while(a->getPC()!=0x4338);
  check(!b->interpretedSteps&&!shared.arpEventCalls.partialEntries&&!shared.arpPrimitiveCalls.partialEntries,"Native arp event used recovery");check(shared.arpEventCalls.completed>=1,"Native arp event did not complete");if(lifecycle)check(shared.arpEventCalls.started==shared.arpEventCalls.completed&&!shared.arpEventCalls.activeCalls(),"Arp event lifecycle is unbalanced");check(std::memcmp(a->memory+0x15c00,b->memory+0x15c00,0x40000-0x15c00)==0,"Arp event memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned kind=0;kind<7;++kind)for(unsigned flags=0;flags<256;++flags)verify(kind,kind==4?7:kind==6?flags:3,kind==4?2:2,flags,flags,flags%6,flags,0,0,flags,clockOnly,flags&1,unsigned(-1));
  for(unsigned kind:{0u,1u,2u})for(unsigned value1=0;value1<256;++value1)for(unsigned value2:{0u,1u,2u,255u})verify(kind,value1,value2,0,0,0,0,0,0,0xd0,clockOnly,value1&1,unsigned(-1));
  for(unsigned policy=0;policy<256;++policy)for(unsigned kind:{3u,5u,6u})verify(kind,60,192,policy,0,1,1,0,0,0xd0,clockOnly,policy&1,unsigned(-1));
  for(unsigned note=0;note<256;++note)for(unsigned shape:{0u,1u,10u,21u,127u,255u})verify(5,note,192,0,0,shape,0,0,0,0xd0,clockOnly,note&1,unsigned(-1));
  for(unsigned phase:{0u,1u,127u,32767u,32768u,65535u})for(unsigned expiry:{0u,1u,127u,32767u,32768u,65535u})verify(1,2,1,0,0,1,0,phase,expiry,0xd0,clockOnly,phase&1,unsigned(-1));
  for(unsigned value1=0;value1<=32;++value1)for(unsigned value2=0;value2<=32;++value2)verify(4,value1,value2,4,32,0,0,0,0,0xd0,clockOnly,value1&1,unsigned(-1));
  for(unsigned kind=0;kind<7;++kind){first.fill(0);verify(kind,kind==4?7:3,kind==4?2:2,4,kind==4?32:0,1,0,0,0,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::ArpEventCalls::candidate(pc))verify(kind,kind==4?7:3,kind==4?2:2,4,kind==4?32:0,1,0,0,0,0xe7,clockOnly,true,selected[pc]-1);verify(kind,kind==4?7:3,kind==4?2:2,4,kind==4?32:0,1,0,0,0,0xd0,clockOnly,true,unsigned(-1),true);verify(kind,kind==4?7:3,kind==4?2:2,4,kind==4?32:0,1,0,0,0,0xd0,clockOnly,true,unsigned(-1),false,true);}
  for(unsigned kind:{3u,5u})for(unsigned policy:{32u,64u}){first.fill(0);verify(kind,60,192,policy,0,0,1,0,0,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::ArpEventCalls::candidate(pc))verify(kind,60,192,policy,0,0,1,0,0,0xe7,clockOnly,true,selected[pc]-1);}
  for(unsigned kind:{1u,2u})verify(kind,2,2,0,0,1,0,0,0,0xd0,clockOnly,true,unsigned(-1),false,false,true);
  for(unsigned velocity:{1u,64u,127u})verify(5,60,velocity,0,0,0,1,0,0,0xd0,clockOnly,true,unsigned(-1));
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final native arp event memory differs");std::cout<<"PASS: "<<cases<<" actual native arp queue/gate/chord/note/reset calls, "<<steps<<" independent original steps; exact accesses/clocks/state/memory; "<<irqCases<<" IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
