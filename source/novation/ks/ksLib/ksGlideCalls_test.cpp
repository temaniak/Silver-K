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
    check(a.getPC()==b.getPC(),"glide call PC projection differs");
    check(a.cycles==b.cycles,"glide call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"glide call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("glide call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"glide call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"glide call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original glide fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original glide fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original glide fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("glide call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Glide fixture linked synchronized recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b535047);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x9300> first{};
 auto verify=[&](bool relative,unsigned time,unsigned mode,unsigned allocation,unsigned distanceMode,unsigned distance,unsigned pitch,unsigned detune,unsigned note,unsigned base,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned slot=13,bool tagged=false){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.parameterScaleCalls.discardCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  const unsigned stack=odd?0xfff001u:0xa18000u;const uint32_t patch=tagged?0xffa30000u:0x30000u;const unsigned physical=patch&0xffffff,entry=relative?0x9138:0x8e9c;
  a->regs[6].er=b->regs[6].er=int32_t(patch);a->regs[5].er=b->regs[5].er=int32_t(flags&1?0x400000:0x600000);a->regs[4].r=b->regs[4].r=0x8c;
  a->setSP(stack);b->setSP(stack);for(auto* m:{a->memory,b->memory}){
   m[0x400004]=m[0x600004]=7;m[0x174a7]=uint8_t(slot);m[0x17526]=2;m[0x1752a]=uint8_t(allocation);m[0x1752d]=0x7f;
   m[0x17560 + slot*20+13]=uint8_t(note);m[0x17560 + slot*20+7]=uint8_t(distance);
   m[physical]=uint8_t(mode);m[physical+3]=uint8_t(time);m[physical+4]=uint8_t(detune);m[physical+5]=uint8_t(distanceMode);m[physical+168]=uint8_t(pitch);
   m[0x1513c]=uint8_t(base>>8);m[0x1513d]=uint8_t(base);m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;uint64_t completionFloor=0;bool injected=false,returned=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" relative="<<relative<<" time="<<time<<" mode="<<mode<<" allocation="<<allocation<<" distanceMode="<<distanceMode<<" distance="<<distance<<" pitch="<<pitch<<" detune="<<detune<<" note="<<note<<" base="<<base<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count%97==5&&!returned){shared.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){m[0x174a7]=uint8_t((slot+count)%256);m[0x1752a]=uint8_t(count%7);m[0x1752d]=uint8_t(count*11);
    m[physical]=uint8_t(count*13);m[physical+3]=uint8_t(count*17);m[physical+4]=uint8_t(count*19);m[physical+5]=uint8_t(count%3);m[physical+168]=uint8_t(count%5?0:count*23);
    m[0x1513c]=uint8_t(count*29);m[0x1513d]=uint8_t(count*31);for(unsigned v=0;v<256;++v){m[0x17560 + v*20+13]=uint8_t(note+count*37+v);m[0x17560 + v*20+7]=uint8_t(distance+count*41+v);}}
   step();check(++count<4096,"Native glide stalled");
   if(relative&&(a->getPC()==0x9054||a->getPC()==0x927e)&&a->getSP()==stack)break;
   if(!relative&&returned&&shared.voiceActions.glideCalls.completed>completionFloor)break;
   if(!relative&&!returned&&a->getPC()==0x8fb6&&a->getSP()==stack){returned=true;completionFloor=shared.voiceActions.glideCalls.completed;}
   if(nested&&count==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-512;
    a->setSP(child);b->setSP(child);a->pc=a->makepc(0x9138);b->pc=b->makepc(0x9138);a->regs[6].er=b->regs[6].er=int32_t(patch);unsigned childCount=0;
    do{step();check(++childCount<4096,"Nested glide stalled");if(childCount==5){shared.resetMetrics();++resets;}}while((a->getPC()!=0x9054&&a->getPC()!=0x927e)||a->getSP()!=child);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(true);
  check(!b->interpretedSteps&&!shared.voiceActions.glideCalls.partialEntries&&!shared.noteActions.partControlCalls.partialEntries&&!shared.voiceActions.wordCalls.partialEntries&&!shared.voiceActions.routingCalls.partialEntries&&!shared.parameterScaleCalls.partialEntries,"Glide used interpreter/recovery");
  const auto& calls=shared.voiceActions.glideCalls;check(calls.completed>=1,"Glide did not complete");check(calls.started+calls.initialActive==calls.completed+calls.activeCalls(),"Glide lifetime accounting differs");
  check(std::memcmp(a->memory+0x1513c,b->memory+0x1513c,0x19000-0x1513c)==0,"Glide memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true})for(bool relative:{false,true}){
  for(unsigned time=0;time<256;++time)for(unsigned mode=0;mode<4;++mode)for(unsigned allocation=0;allocation<6;++allocation)for(unsigned distanceMode:{0u,1u})for(unsigned distance:{0u,1u,127u,255u})verify(relative,time,mode<<3,allocation,distanceMode,distance,0,64,60,0x4470,(time+mode+allocation)&255,clockOnly,bool(time&1),unsigned(-1));
  for(unsigned detune=0;detune<256;++detune)for(unsigned note=0;note<256;++note)verify(relative,127,0,3,1,7,0,detune,note,0x4470,(detune+note*149)&255,clockOnly,bool(note&1),unsigned(-1));
  for(unsigned base=0;base<65536;++base)verify(relative,base&255,0,3,base&1,base&255,127,64,60,base,base&255,clockOnly,bool(base&1),unsigned(-1));
  for(unsigned slot:{0u,13u,255u})for(bool tagged:{false,true})for(unsigned flags=0;flags<256;++flags)verify(relative,255,flags,flags,1,255,255,192,255,0x8000,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,slot,tagged);
  for(unsigned time:{0u,1u,127u,128u,255u})for(unsigned distanceMode:{0u,1u}){
   first.fill(0);verify(relative,time,0,3,distanceMode,255,0,63,60,0x4470,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::GlideCalls::candidate(pc))verify(relative,time,0,3,distanceMode,255,0,63,60,0x4470,0xe7,clockOnly,true,selected[pc]-1);
   verify(relative,time,8,1,distanceMode,0,0,192,255,0xffff,0xd0,clockOnly,true,unsigned(-1),true);
  }
  for(unsigned time:{0u,1u,255u})verify(relative,time,0,3,1,255,0,63,60,0x4470,0xd0,clockOnly,true,unsigned(-1),false,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Glide final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual glide jobs/caller admissions, "<<steps<<" independent original steps; time/mode/allocation/distance paths, all detune/note pairs and static bases, flags/slot/patch bounds, both fetch modes/alignments, live fields; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested jobs; exact data/clocks/state/full memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
