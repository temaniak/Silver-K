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
    check(a.getPC()==b.getPC(),"voice-tracking call PC projection differs");
    check(a.cycles==b.cycles,"voice-tracking call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"voice-tracking call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("voice-tracking call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"voice-tracking call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"voice-tracking call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original voice-tracking fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original voice-tracking fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original voice-tracking fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("voice-tracking call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Tracking fixture linked synchronized factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b535452);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0xa340> first{};
 auto store32=[](uint8_t* m,unsigned at,uint32_t value){for(unsigned i=0;i<4;++i)m[at+i]=uint8_t(value>>(24-8*i));};
 auto verify=[&](unsigned entry,uint32_t pointer,unsigned shape,unsigned sample0,unsigned sample1,unsigned time,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned part=2,unsigned tracked=13,bool tagged=false){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());a->regs[6].er=b->regs[6].er=int32_t(pointer);
  const unsigned stack=odd?0xfff001u:0xa18000u;const uint32_t patch=tagged?0xffa30000u:0x30000u;const unsigned physical=patch&0xffffff;const uint16_t base=uint16_t(time*257u);
  a->setSP(stack);b->setSP(stack);for(auto* m:{a->memory,b->memory}){
   m[0x400004]=m[0x600004]=7;m[0x17526]=uint8_t(part);m[0x16798]=uint8_t(shape==1?part+1:part);m[0x174a7]=uint8_t(shape==2?tracked+1:tracked);m[0x1755e]=uint8_t(tracked);
   store32(m,0x167bc,patch);m[physical+73]=uint8_t(sample0);m[physical+76]=uint8_t(sample1);m[physical+72]=uint8_t(time);m[physical+75]=uint8_t(255-time);m[0x15138]=uint8_t(base>>8);m[0x15139]=uint8_t(base);
   for(unsigned n=0;n<64;++n)store32(m,0x159f4 + n*4,uint32_t(n*0x31415927u+time*0x8badf00du));
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<" entry="<<entry<<" pointer="<<pointer<<std::dec<<" shape="<<shape<<" sample="<<sample0<<','<<sample1<<" time="<<time<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count%97==5){shared.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){m[0x17526]=uint8_t(count%4);m[0x16798]=uint8_t((count/3)%4);m[0x174a7]=uint8_t(count%14);m[0x1755e]=uint8_t(count%17);
    m[physical+73]=uint8_t(count%3?0:count);m[physical+76]=uint8_t(count%5?0:count);m[physical+72]=uint8_t(count*17);m[physical+75]=uint8_t(count*23);m[0x15138]=uint8_t(count*31);m[0x15139]=uint8_t(count*37);
    for(unsigned n=0;n<64;++n)store32(m,0x159f4 + n*4,uint32_t(count*0x8badf00du+n*0x31415927u));}
   step();check(++count<8192,"Native tracking stalled");
   if(nested&&count==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->regs[6].er=b->regs[6].er=int32_t(pointer);unsigned childCount=0;
    do{step();check(++childCount<8192,"Nested tracking stalled");if(childCount==5){shared.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=0x4338);
  check(!b->interpretedSteps&&!shared.voiceActions.trackingCalls.partialEntries&&!shared.parameterScaleCalls.partialEntries&&!shared.voiceActions.routingCalls.partialEntries&&!shared.voiceActions.wordCalls.partialEntries,"Tracking used interpreter/recovery");
  check(shared.voiceActions.trackingCalls.started+shared.voiceActions.trackingCalls.initialActive==shared.voiceActions.trackingCalls.completed+shared.voiceActions.trackingCalls.activeCalls(),"Tracking lifetime accounting differs");
  check(shared.voiceActions.trackingCalls.completed>=1,"Tracking task did not complete");check(std::memcmp(a->memory+0x159f4,b->memory+0x159f4,0x17600-0x159f4)==0,"Tracking data memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true}){
  for(uint32_t pointer:{0u,0x30000u,0x838fffu,0x839000u,0x839001u,0xffa30000u,0xffffffffu})for(unsigned shape=0;shape<3;++shape)for(unsigned flags=0;flags<256;++flags)verify(0xa1b0,pointer,shape,0,0,64,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,flags%4,flags,bool(flags&2));
  for(unsigned byte=0;byte<256;++byte)verify(0xa1fa,0x12340000u+(byte<<8)+0x5a,0,0,0,64,byte,clockOnly,bool(byte&1),unsigned(-1));
  for(unsigned sample0:{0u,1u,64u,65u,127u,128u,255u})for(unsigned sample1:{0u,1u,64u,65u,127u,128u,255u})for(unsigned time=0;time<256;++time)verify(0xa208,0x30000,0,sample0,sample1,time,time,clockOnly,bool(time&1),unsigned(-1),false,false,2,13,bool(time&2));
  for(unsigned sample=0;sample<256;++sample)verify(0xa208,0x30000,0,sample,255-sample,sample&1?0:255,sample,clockOnly,bool(sample&1),unsigned(-1));
  for(unsigned flags=0;flags<256;++flags)for(unsigned shape:{0u,1u})verify(0xa208,0x30000,shape,0,255,flags,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,flags%4,flags);
  for(unsigned entry:{0xa1b0u,0xa1fau,0xa208u})for(unsigned sample:{0u,1u,255u}){
   first.fill(0);verify(entry,0x30000,0,sample,sample,255,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::VoiceTrackingCalls::candidate(pc))verify(entry,0x30000,0,sample,sample,255,0xe7,clockOnly,true,selected[pc]-1);
   verify(entry,0x30000,0,sample,sample,255,0xd0,clockOnly,true,unsigned(-1),true);verify(entry,0x30000,0,sample,sample,255,0xd0,clockOnly,true,unsigned(-1),false,true);
  }
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Tracking final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual native tracking/aux/envelope calls, "<<steps<<" independent original steps; patch domain/part/tracked boundaries, all sample/time/flag bytes, low/tagged bindings, both fetch modes/stack alignments, live fields/tables; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested jobs; exact accesses/clocks/state/full memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
