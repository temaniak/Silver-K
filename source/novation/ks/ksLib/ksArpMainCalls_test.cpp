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
 auto verify=[&](unsigned modeIndex,unsigned policy,unsigned oldPolicy,unsigned pedal,unsigned phase,unsigned gate,unsigned delay,unsigned flags,unsigned shape,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false){
  shared.arpMainCalls.discardCalls();shared.arpScheduleCalls.discardCalls();shared.arpPrimitiveCalls.discardCalls();shared.midiRouteCalls.discardCalls();shared.midiAuxCalls.discardCalls();shared.midiControlCalls.discardCalls();shared.noteActions.pedalNativeCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();shared.arpMainCalls.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u,part=shape%5,base=0x160d2 + part*334;
  const uint32_t pattern=std::array<uint32_t,6>{0x31000,0x31001,0xa31000,0xa31001,0xff031000u,0xffa31001u}[(shape/5)%6];
  a->setSP(stack);b->setSP(stack);
  for(auto* m:{a->memory,b->memory}){std::fill(m+0x15c00,m+0x40000,0);
   for(unsigned p=0;p<5;++p){const unsigned pb=0x160d2 + p*334;const auto ctl=0x174ae + p*20;m[ctl]=0;m[ctl+1]=3;m[ctl+2]=m[ctl+3]=0;m[pb+324]=m[62696+modeIndex];m[pb+317]=uint8_t(gate);m[pb+315]=8;m[pb+327]=uint8_t(oldPolicy);m[pb+310]=uint8_t(pedal);
    auto put=[&](unsigned at,uint32_t value){for(unsigned i=0;i<4;++i)m[at+i]=uint8_t(value>>(24-i*8));};put(pb+8,pattern);put(pb+4,0x10000);put(pb+60,uint32_t(phase)<<16);}
   for(unsigned i=0;i<32;++i)m[(pattern+i)&0xffffff]=uint8_t(i&1?delay+i:shape/30==1?0:i==4&&shape%10<5?0:1);
   m[0x30000+85]=0;m[0x30000+86]=uint8_t(flags);m[0x30000+87]=uint8_t(modeIndex);m[0x30000+88]=uint8_t(policy);m[0x30000+149]=0;m[0x30000+167]=97;
   if(shape/30==2)m[base+324]^=64;if(shape/30==3)m[base+318]=1;
   std::fill(m+0x17560,m+0x17574,0);m[0x174a6]=1;m[0x400004]=m[0x600004]=7;m[0x17526]=uint8_t(part);m[0x160d0]=0;
   for(unsigned i=0;i<4;++i)m[0xffee78+i]=0;
   if(shape>=120){const std::array<uint32_t,12> edges{0,1,65535,65536,0x7fffffffu,0x80000000u,0xffffffffu,0x40000000u,0x12345678u,0xffff0001u,0x0001ffffu,0xdeadbeefu};auto put=[&](unsigned at,uint32_t value){for(unsigned i=0;i<4;++i)m[at+i]=uint8_t(value>>(24-i*8));};put(0xffee78,edges[delay%12]);put(base,edges[flags%12]);put(base+60,edges[(delay+flags)%12]);put(base+4,edges[(delay*3+flags)%12]);}
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(0x3770);b->pc=b->makepc(0x3770);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned countSteps=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" modeIndex="<<modeIndex<<" policy="<<policy<<" oldPolicy="<<oldPolicy<<" pedal="<<pedal<<" phase="<<phase<<" gate="<<gate<<" delay="<<delay<<" flags="<<flags<<" shape="<<shape<<'\n';throw;}if(before<first.size()&&!first[before])first[before]=countSteps+1;++steps;};
  do{
   if(countSteps==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(countSteps%7==5){shared.resetMetrics();shared.arpMainCalls.resetMetrics();++resets;}
   if(mutate&&a->getPC()==0x38a6)for(auto* m:{a->memory,b->memory})m[0xffee7b]=uint8_t(countSteps);
   step();check(++countSteps<300000,"Native arp main stalled");
   if(nested&&countSteps==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->pc=a->makepc(0x3770);b->pc=b->makepc(0x3770);unsigned childCount=0;do{step();check(++childCount<300000,"Nested arp main stalled");if(childCount==5){shared.resetMetrics();shared.arpMainCalls.resetMetrics();++resets;}}while(a->getPC()!=0x4338&&a->getPC()!=0xb444);for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;}
  }while(a->getPC()!=0x4338&&a->getPC()!=0xb444);
  check(!b->interpretedSteps&&!shared.arpMainCalls.partialEntries&&!shared.arpPrimitiveCalls.partialEntries,"Native arp main used recovery");check(shared.arpMainCalls.completed+shared.arpMainCalls.handedOff>=1,"Native arp main did not complete/handoff");check(std::memcmp(a->memory+0x15c00,b->memory+0x15c00,0x40000-0x15c00)==0,"Arp main memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned flags=0;flags<256;++flags)for(unsigned delay:{0u,1u,63u,127u,255u})for(unsigned shape:{0u,5u,10u,15u,20u,25u,30u})verify(0,0,0,0,0,0,delay,flags,shape,clockOnly,flags&1,unsigned(-1));
  for(unsigned phase=0;phase<256;++phase)for(unsigned gate:{0u,1u,127u,128u,255u})verify(0,0,0,0,phase<<8,gate,23,64,0,clockOnly,phase&1,unsigned(-1));
  for(unsigned policy=0;policy<256;++policy)for(unsigned shape:{0u,60u,90u})verify(0,policy,uint8_t(~policy),policy,0,0,17,64,shape,clockOnly,policy&1,unsigned(-1));
  for(unsigned modeIndex=0;modeIndex<64;++modeIndex)for(unsigned shape:{0u,60u,90u})verify(modeIndex,0,0,0,0,128,11,64,shape,clockOnly,modeIndex&1,unsigned(-1));
  for(unsigned modeIndex=0;modeIndex<64;++modeIndex)for(unsigned policy:{0u,4u,16u,20u,64u,127u,128u,255u})verify(modeIndex,policy,uint8_t(~policy),policy,0,0,17,64,60,clockOnly,policy&1,unsigned(-1));
  for(unsigned delay=0;delay<12;++delay)for(unsigned flags=0;flags<12;++flags)verify(0,0,0,0,0,0,delay,flags,120,clockOnly,delay&1,unsigned(-1));
  for(unsigned shape:{0u,5u,30u,60u,90u}){first.fill(0);verify(0,4,20,16,0,0,23,64,shape,clockOnly,false,unsigned(-1));const auto selected=first;for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::ArpMainCalls::candidate(pc))verify(0,4,20,16,0,0,23,64,shape,clockOnly,true,selected[pc]-1);}
  verify(0,0,0,0,0,0,23,64,0,clockOnly,true,unsigned(-1),true);verify(0,0,0,0,0,0,23,64,0,clockOnly,true,unsigned(-1),false,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final native arp main memory differs");std::cout<<"PASS: "<<cases<<" actual native arp main calls, "<<steps<<" independent original steps; exact accesses/clocks/state/memory; "<<irqCases<<" IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
