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
#include "ksArpSelectCalls.h"
#include "ksInternalMidiCalls.h"
#include "ksInternalNoteCalls.h"
#include "ksSysExFrameCalls.h"
#include "ksPatchExpandCalls.h"
#include "ksSysExActionCalls.h"
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
 uint8_t read(uint32_t a)override{uint8_t value=this->state->memory[a];if(a==0xffff8c)value|=128;else if(a==0x40000a||a==0x60000a)value=pitch;else if(a==0x40000c||a==0x60000c)value=middle;else if(a==0x40000e||a==0x60000e)value=low;events.emplace_back(false,a,value,this->state->cycles,uint32_t(this->state->getPC()));return value;}
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
 std::mt19937 rng(0x4b53524b);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0xb900> first{};
 uint64_t seams=0;auto verify=[&](unsigned kind,unsigned mode,unsigned bank,unsigned program,unsigned count,unsigned shape,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false){
  shared.sysExActionCalls.discardCalls();shared.patchExpandCalls.discardCalls();shared.sysExFrameCalls.discardCalls();shared.internalNoteCalls.discardCalls();shared.internalMidiCalls.discardCalls();shared.arpSelectCalls.discardCalls();shared.arpEventCalls.discardCalls();shared.arpMainCalls.discardCalls();shared.arpScheduleCalls.discardCalls();shared.arpPrimitiveCalls.discardCalls();shared.midiRouteCalls.discardCalls();shared.midiAuxCalls.discardCalls();shared.midiControlCalls.discardCalls();shared.noteActions.pedalNativeCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();shared.sysExActionCalls.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u,entry=std::array<unsigned,11>{0xab42,0xab6c,0xab94,0xaba8,0xabca,0xabea,0xac08,0xac3a,0xac6c,0xac88,0xac96}[kind];const uint32_t source=std::array<uint32_t,6>{0x31000,0x31001,0xa31000,0xa31001,0xff031000u,0xffa31001u}[shape%6];
  a->setSP(stack);b->setSP(stack);a->regs[0].er=b->regs[0].er=0x16040;a->regs[1].er=b->regs[1].er=0x15c38;a->regs[2].rl=b->regs[2].rl=int8_t(program);a->regs[5].r=b->regs[5].r=int16_t(count);a->regs[6].er=b->regs[6].er=int32_t(source);
  for(auto* m:{a->memory,b->memory}){std::fill(m+0x15c00,m+0x40000,0);for(unsigned i=0;i<2048;++i)m[(source+i)&0xffffff]=uint8_t(i*17+program);m[112]=m[113]=0;m[114]=1;m[115]=0x70;m[0x16040]=uint8_t(bank);m[0x16041]=uint8_t(program);m[0x1603d]=uint8_t(shape&1);m[0x167ac]=uint8_t(bank);m[0x167ba]=uint8_t(mode);m[0x167bc]=0;m[0x167bd]=1;m[0x167be]=mode?0x69:0x68;m[0x167bf]=0xfc;m[0x174a6]=1;m[0x17528]=1;m[0x400004]=m[0x600004]=7;m[0xffed6c]=m[0xffed6d]=m[0xffed6e]=m[0xffed6f]=0;
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned countSteps=0;bool injected=false,seenTermination=false,handed=false,seam=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);seenTermination|=shared.sysExActionCalls.completed>0;handed|=shared.sysExActionCalls.handedOff>0;a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" kind="<<kind<<" mode="<<mode<<" bank="<<bank<<" program="<<program<<" count="<<count<<" shape="<<shape<<" flags="<<flags<<'\n';throw;}if(before<first.size()&&!first[before])first[before]=countSteps+1;++steps;};
  do{
   if(a->getPC()==0x63fe||a->getPC()==0x6ffc||a->getPC()==0x64e4||a->getPC()==0x72e0||a->getPC()==0x66dc){seam=true;break;}
   if(countSteps==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(countSteps%17==5){shared.resetMetrics();shared.sysExActionCalls.resetMetrics();++resets;}
   if(a->getPC()==0x1124)for(auto* m:{a->memory,b->memory}){m[0xffed6e]=m[0xffed6c];m[0xffed6f]=m[0xffed6d];}
   if(mutate)for(auto* m:{a->memory,b->memory}){if(a->getPC()==0xabbc)m[0x16041]=uint8_t(countSteps);if(a->getPC()==0xac88)m[source&0xffffff]=uint8_t(countSteps);}
   step();if(++countSteps>=5000000){std::cerr<<"stalled="<<std::hex<<a->getPC()<<std::dec<<" kind="<<kind<<" bank="<<bank<<" program="<<program<<'\n';throw std::runtime_error("Native SysEx action stalled");}
   if(nested&&countSteps==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}a->pc=a->makepc(0xac6c);b->pc=b->makepc(0xac6c);unsigned childCount=0;do{step();check(++childCount<50000,"Nested SysEx action stalled");if(childCount==5){shared.resetMetrics();shared.sysExActionCalls.resetMetrics();++resets;}}while(a->getPC()!=0x4338);for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;}
  }while(a->getPC()!=0x4338&&!handed);
  check(!b->interpretedSteps&&!shared.sysExActionCalls.partialEntries,"Native SysEx action used recovery");check(seenTermination||handed||seam,"Native SysEx action did not complete");check(std::memcmp(a->memory+0x15c00,b->memory+0x15c00,0x40000-0x15c00)==0,"SysEx action memory differs");++cases;if(injected)++irqCases;if(handed||seam)++seams;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned kind=0;kind<11;++kind)for(unsigned flags=0;flags<256;++flags)verify(kind,flags&1,flags,flags,kind==9?7:12,flags%6,flags,clockOnly,flags&1,unsigned(-1));
  for(unsigned bank=0;bank<256;++bank)for(unsigned program:{0u,49u,50u,99u,100u,255u})verify(3,0,bank,program,12,0,0xd0,clockOnly,bank&1,unsigned(-1));
  for(unsigned program=0;program<256;++program)verify(4,1,1,program,12,0,0xd0,clockOnly,program&1,unsigned(-1));
  for(unsigned count:{0u,1u,2u,12u,127u,255u,256u,1024u})verify(9,0,1,7,count,0,0xd0,clockOnly,count&1,unsigned(-1));
  for(unsigned kind=0;kind<11;++kind){first.fill(0);verify(kind,kind==1?1:0,1,7,kind==9?3:12,0,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::SysExActionCalls::candidate(pc))verify(kind,kind==1?1:0,1,7,kind==9?3:12,0,0xe7,clockOnly,true,selected[pc]-1);verify(kind,0,1,7,kind==9?3:12,0,0xd0,clockOnly,true,unsigned(-1),true);verify(kind,0,1,7,kind==9?3:12,0,0xd0,clockOnly,true,unsigned(-1),false,true);}
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final SysEx action memory differs");std::cout<<"PASS: "<<cases<<" actual native SysEx apply/metadata/header/send calls, "<<steps<<" independent original steps; exact accesses/clocks/state/memory; "<<irqCases<<" IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested, "<<seams<<" exact commit/notification seams; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
