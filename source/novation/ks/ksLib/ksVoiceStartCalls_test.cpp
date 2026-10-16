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
    check(a.getPC()==b.getPC(),"voice-start call PC projection differs");
    check(a.cycles==b.cycles,"voice-start call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"voice-start call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("voice-start call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"voice-start call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"voice-start call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original voice-start fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original voice-start fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original voice-start fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("voice-start call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Voice start fixture linked synchronized recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b535653);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0,handed=0,suppressed=0;std::array<unsigned,0x9300> first{};
 auto store32=[](uint8_t* m,unsigned at,uint32_t value){for(unsigned i=0;i<4;++i)m[at+i]=uint8_t(value>>(24-8*i));};
 auto verify=[&](unsigned entry,unsigned mode,unsigned shape,unsigned velocity,unsigned flags,unsigned middle,unsigned low,unsigned detune,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned part=2,unsigned slot=13,unsigned note=60,bool tagged=false){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.parameterScaleCalls.discardCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u;const uint32_t patch=tagged?0xffa30000u:0x30000u;const unsigned physical=patch&0xffffff;
  a->regs[6].er=b->regs[6].er=int32_t(patch);a->regs[5].er=b->regs[5].er=int32_t(flags&1?0x400000:0x600000);a->regs[4].r=b->regs[4].r=0x8c;
  a->setSP(stack);b->setSP(stack);for(auto* m:{a->memory,b->memory}){
   m[0x400004]=m[0x600004]=7;m[0x17526]=uint8_t(part);m[0x16798]=uint8_t(part);m[0x174a6]=14;m[0x174a7]=uint8_t(slot);m[0x17528]=uint8_t(note);m[0x17529]=uint8_t(velocity);
   m[0x169f1]=0;m[0x169f0]=64;m[0x17516]=1;m[0x17517]=0;m[0x1755e]=0xff;
   for(unsigned p=0;p<4;++p){const unsigned at=0x174ae + p*20;for(unsigned n=0;n<20;++n)m[at+n]=0;store32(m,at,patch);m[at+14]=64;m[at+17]=uint8_t(shape==3);}
   for(unsigned n=0;n<256;++n)m[physical+n]=0;m[physical]=uint8_t(mode);m[physical+4]=uint8_t(detune);m[physical+79]=uint8_t(flags);m[physical+78]=m[physical+147]=uint8_t(middle);m[physical+80]=uint8_t(low);
   for(unsigned v=0;v<256;++v){const unsigned at=0x17560 + v*20;for(unsigned n=0;n<20;++n)m[at+n]=0;store32(m,at,patch);store32(m,at+8,shape==0?0:0x80000000u+v*0x10000u+0xffffu);m[at+4]=uint8_t(shape==2?part+1:part);m[at+5]=uint8_t(shape&1?note:note+v);m[at+13]=uint8_t(note);m[at+7]=7;}
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  x.pitch=y.pitch=127;x.middle=y.middle=x.low=y.low=0;
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false,wasSuppressed=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());if(before==0x92be)wasSuppressed=true;x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<" entry="<<entry<<std::dec<<" mode="<<mode<<" shape="<<shape<<" velocity="<<velocity<<" flags="<<flags<<" middle="<<middle<<" low="<<low<<" detune="<<detune<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count%97==5){shared.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){m[0x1752a]=uint8_t(count%5);m[physical+79]=uint8_t(flags+count*17);m[physical+78]=m[physical+147]=uint8_t(middle+count*23);m[physical+80]=uint8_t(low+count*29);m[physical+4]=uint8_t(detune+count*31);}
   step();check(++count<30000,"Native voice start stalled");
   if(a->getPC()==0x4338||(a->getPC()==0x9084&&a->getSP()==stack))break;
   if(nested&&count==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-512;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}a->pc=a->makepc(0x927e);b->pc=b->makepc(0x927e);a->regs[6].er=b->regs[6].er=int32_t(patch);unsigned childCount=0;
    do{step();check(++childCount<4096,"Nested voice gate stalled");if(childCount==5){shared.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(true);
  check(!b->interpretedSteps&&!shared.voiceActions.startCallsNative.partialEntries&&!shared.voiceActions.populationCalls.partialEntries&&!shared.voiceActions.selectionCalls.partialEntries&&!shared.voiceActions.trackingCalls.partialEntries&&!shared.voiceActions.glideCalls.partialEntries&&!shared.voiceActions.modulationCalls.partialEntries,"Voice start used interpreter/recovery");
  const auto& calls=shared.voiceActions.startCallsNative;check(calls.completed>=1,"Voice start did not finish");check(calls.started+calls.initialActive==calls.completed+calls.activeCalls(),"Voice start lifetime accounting differs");
  check(std::memcmp(a->memory+0x159f0,b->memory+0x159f0,0x19000-0x159f0)==0,"Voice start memory differs");++cases;if(injected)++irqCases;if(wasSuppressed)++suppressed;if(a->getPC()==0x9084)++handed;
 };
 for(bool clockOnly:{false,true}){
  for(unsigned entry:{0x9054u,0x927eu}){
   for(unsigned flags=0;flags<256;++flags)for(unsigned middle=0;middle<256;++middle)verify(entry,0,0,100,flags,middle,flags^middle,flags&1?64:255,clockOnly,bool(flags&1),unsigned(-1));
   for(unsigned low=0;low<256;++low)for(unsigned detune=0;detune<256;++detune)verify(entry,0,0,100,255,255,low,detune,clockOnly,bool(low&1),unsigned(-1));
  }
  for(unsigned mode=0;mode<32;++mode)for(unsigned shape=0;shape<4;++shape)for(unsigned velocity:{0u,1u,63u,64u,127u,128u,192u,255u})verify(0x902a,mode,shape,velocity,0xd0,0xa5,0x37,64,clockOnly,false,unsigned(-1));
  for(unsigned mode:{0u,8u,16u,24u})for(unsigned velocity=0;velocity<256;++velocity)verify(0x902a,mode,0,velocity,velocity,0xa5,0x37,64,clockOnly,bool(velocity&1),unsigned(-1));
  for(unsigned entry:{0x902au,0x9054u,0x927eu})for(unsigned part=0;part<4;++part)for(bool tagged:{false,true})for(unsigned flags=0;flags<256;++flags)verify(entry,16,0,100,flags,flags^0xa5,flags^0x5a,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,part,flags&1?0:255,flags,tagged);
  for(unsigned entry:{0x902au,0x9054u,0x927eu})for(unsigned shape:{0u,1u}){
   first.fill(0);verify(entry,shape?16:0,shape,100,0xd0,0xa5,0x37,shape?64:0,clockOnly,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::VoiceStartCalls::candidate(pc))verify(entry,shape?16:0,shape,100,0xe7,0xa5,0x37,shape?64:0,clockOnly,true,selected[pc]-1);
   verify(entry,0,0,100,0xd0,0xa5,0x37,0,clockOnly,true,unsigned(-1),true);verify(entry,0,0,100,0xd0,0xa5,0x37,0,clockOnly,true,unsigned(-1),false,true);
  }
 }
 check(handed&&suppressed,"Start exit paths not covered");check(std::memcmp(a->memory,b->memory,1<<24)==0,"Voice start final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual start/gate jobs, "<<steps<<" independent original steps; all gate flags/middle and low/detune pairs, start modes/ages/velocities, part/tag/slot/flag bounds, both fetch modes/alignments and live packet reads; "<<irqCases<<" IRQ points, "<<resets<<" resets, "<<nestedCases<<" nested jobs, "<<handed<<" modulation handoffs, "<<suppressed<<" suppressed starts; exact data/clocks/state/full memory; zero interpreter/recovery; full parameter child remains compatibility\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
