#include "ksRuntimeController.h"
#include "ksStateStoreUiCalls.h"
#include "ksLiveMidiController.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <cstdlib>
#include <new>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>
namespace {bool trackAllocations=false;uint64_t nativeAllocations=0;}
void* operator new(std::size_t count){if(trackAllocations)++nativeAllocations;if(auto* p=std::malloc(count?count:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t count){return ::operator new(count);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
namespace {
std::unique_ptr<H8SDisassembler> lengthOracle;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
using Access=std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>;
struct Original final:H8SEmulator {using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
template<class Device>struct MockBus final:Device {
 std::vector<Access> events;
 uint8_t read(uint32_t address)override{
  const uint8_t value=address==0x400004||address==0x600004?6:address==0x40000a||address==0x60000a?0:address==0x40000c||address==0x60000c?0x55:address==0x40000e||address==0x60000e?0xaa:this->state->memory[address];
  events.emplace_back(false,address,value,this->state->cycles,uint32_t(this->state->getPC()));return value;
 }
 void write(uint32_t address,uint8_t value)override{
  events.emplace_back(true,address,value,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[address]=value;
 }
};
using OriginalBus=MockBus<H8SDevice>;using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"Store UI call PC projection differs");
    check(a.cycles==b.cycles,"Store UI call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"Store UI call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("Store UI call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"Store UI call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"Store UI call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original Store UI fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original Store UI fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original Store UI fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("Store UI call data/stack/value/order/clock/in-flight context differs");
    }
}
}



int main(int argc,char** argv){try{
 if(argc!=2)return 77;
 check(!ks::controllerReferenceAvailable(),"Independent fixture linked synchronized H8 factory");
 ks::Rom os(argv[1]);check(os.isValid(),"Invalid ROM");
 auto original=std::make_unique<Original>();auto native=std::make_unique<ks::ControllerCpu>();
 lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){original->loadmem(os.data().data(),unsigned(os.data().size()),base);native->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*native);ks::RuntimeController runtime(*native,shared);auto& calls=shared.stateStoreUiCalls;
 OriginalBus oldBus;NativeBus newBus;oldBus.events.reserve(256);newBus.events.reserve(256);
 original->memmap(&oldBus,0,1<<24);native->memmap(&newBus,0,1<<24);native->interpreterAllowed=false;
 std::mt19937 rng(0x53544f52);uint64_t cases=0,steps=0,irqs=0,resets=0,nested=0,paired=0;
 constexpr std::array<unsigned,5> roots{{0x47b8,0x47d6,0x482a,0x484a,0x4858}};
 std::array<unsigned,0x4900> first{};
 auto verify=[&](unsigned kind,unsigned a,unsigned b,unsigned flags,bool clocksOnly,unsigned irqAt,bool mutate,bool nest){
  calls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();
  for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int32_t(rng());
  const unsigned stack=(a&1)?0xfff001u:0xa18000u;
  auto byte=[&](unsigned address,uint8_t value){original->memory[address]=native->memory[address]=value;};
  auto word=[&](unsigned address,uint16_t value){byte(address,uint8_t(value>>8));byte(address+1,uint8_t(value));};
  byte(112,0);byte(113,0);byte(114,1);byte(115,0x70);
  auto dword=[&](unsigned address,uint32_t value){word(address,uint16_t(value>>16));word(address+2,uint16_t(value));};
  const uint32_t patch=0x30000+(b&1),selected=0x34000+((b>>1)&1);
  dword(0x167bc,patch);dword(0x1748e,selected);
  for(unsigned i=0;i<256;++i){byte((patch+i)&0xffffff,uint8_t(a*13+i*17));byte(0x169fc+i,uint8_t(b*23+i*31));byte(selected+i,uint8_t(a*47+i*53));byte(0x820000+i,uint8_t(b*23+i*31));}
  for(unsigned i=0;i<17;++i)byte(0x1733c+i,uint8_t(a*59+i*61));
  for(unsigned i=0;i<4;++i)byte(0xffed6c+i,0);word(0xffed6a,0);
  byte(0x168fc+239,uint8_t(a));byte(0x167ba,uint8_t(b));byte(0x16798,uint8_t(b&3));byte(0x16799,uint8_t(a));byte(0x16790,uint8_t(kind));byte(0x167da,uint8_t(a^b));
  byte(0x167ac,uint8_t(1+(a%8)));byte(0x167ad,uint8_t(b%100));byte(0x167aa,1);byte(0x167ab,uint8_t(b%100));
  original->regs[0].er=native->regs[0].er=0x167ac;
  auto frame=[&](unsigned at){byte(at,0);byte(at+1,0);byte(at+2,0x43);byte(at+3,0x38);};frame(stack);
  original->pc=original->makepc(roots[kind]);native->pc=native->makepc(roots[kind]);original->setSP(stack);native->setSP(stack);
  original->ccr=native->ccr=uint8_t(flags);original->exr=native->exr=0;original->cycles=native->cycles=1000;
  original->lastread=native->lastread=int(rng());original->lastwrite=native->lastwrite=int(rng());original->indent=native->indent=0;
  original->pending_irqs=native->pending_irqs=original->pending_irqs_high=native->pending_irqs_high=0;
  original->interruptMode2=native->interruptMode2=true;
  for(unsigned i=0;i<128;++i)original->interruptPriority[i]=native->interruptPriority[i]=0;
  original->interruptPriority[28]=native->interruptPriority[28]=6;native->nativeFetchClocksOnly=clocksOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{
   const auto before=unsigned(original->getPC());oldBus.events.clear();newBus.events.clear();
   if((flags&1)&&(before==0x47e8||before==0x4816||before==0x483c)){
    const unsigned target=before==0x47e8?(uint32_t(original->regs[1].er)+176)&0xffffff:0x16799;
    if(target&1){original->lastwrite=native->lastwrite=int(target-1);++paired;}
   }
   try{trackAllocations=true;runtime.step(*native);trackAllocations=false;original->step();same(*original,*native,oldBus,newBus,clocksOnly);}
   catch(const std::exception&){trackAllocations=false;std::cerr<<"before="<<std::hex<<before<<std::dec<<" kind="<<kind<<" a="<<a<<" b="<<b<<" flags="<<flags<<" clocks="<<clocksOnly<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;
  };
  do{
   if(count==irqAt){original->pending_irqs=native->pending_irqs=1ull<<28;injected=true;}
   if(count%5==2){shared.resetMetrics();++resets;}
   if(mutate){
    const auto pc=unsigned(original->getPC());
    if(pc==0x47da)dword(0x167bc,patch+0x1000);
    if(pc==0x47e2)byte(0x167da,uint8_t((a^b)+count));
    if(pc==0x4806)byte(0x167ba,uint8_t(b^1));
   }
   step();if(++count>=250000){std::cerr<<"stalled pc="<<std::hex<<original->getPC()<<" sp="<<original->getSP()<<" r5="<<uint32_t(original->regs[5].er)<<std::dec<<" kind="<<kind<<" a="<<a<<" b="<<b<<" flags="<<flags<<" clocks="<<clocksOnly<<" irqAt="<<irqAt<<" mutate="<<mutate<<" nest="<<nest<<'\n';throw std::runtime_error("Store UI stalled");}
   if(nest&&count==2){
    std::array<int32_t,8> oldRegs{},newRegs{};for(unsigned i=0;i<8;++i){oldRegs[i]=original->regs[i].er;newRegs[i]=native->regs[i].er;}
    const auto oldPC=original->pc;const auto newPC=native->pc;const auto oldFlags=original->ccr,newFlags=native->ccr;
    const auto oldMask=original->exr,newMask=native->exr;const auto oldDepth=original->indent,newDepth=native->indent;
    const unsigned child=stack-512;frame(child);original->setSP(child);native->setSP(child);
    original->pc=original->makepc(roots[kind]);native->pc=native->makepc(roots[kind]);
    unsigned childSteps=0;do{step();check(++childSteps<250000,"Nested store UI stalled");}while(original->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){original->regs[i].er=oldRegs[i];native->regs[i].er=newRegs[i];}
    original->pc=oldPC;native->pc=newPC;original->ccr=oldFlags;native->ccr=newFlags;
    original->exr=oldMask;native->exr=newMask;original->indent=oldDepth;native->indent=newDepth;++nested;
   }
  }while(original->getPC()!=0x4338);
  check(native->getSP()==stack+4,"Store UI return frame");
  check(!native->interpretedSteps&&!calls.partialEntries,"Store UI used recovery/interpreter");
  check(calls.started+calls.initialActive==calls.completed+calls.activeCalls()&&!calls.activeCalls(),"Store UI metadata lifetime");
  ++cases;if(injected)++irqs;
 };
 for(bool clocks:{false,true})for(unsigned kind=0;kind<5;++kind){
  for(unsigned a=0;a<16;++a)for(unsigned b:{0u,1u,127u,255u})verify(kind,a,b,(a+b*149)&255,clocks,unsigned(-1),false,false);
  for(unsigned a:{0u,1u,255u})for(unsigned b:{0u,1u}){
   first.fill(0);verify(kind,a,b,0x50,clocks,unsigned(-1),false,false);const auto observed=first;
   for(unsigned pc=0;pc<observed.size();++pc)if(observed[pc]&&ks::StateStoreUiCalls::candidate(pc))verify(kind,a,b,0x50,clocks,observed[pc]-1,true,false);
   verify(kind,a,b,0xa5,clocks,unsigned(-1),true,true);
  }
  std::cout<<"checked store kind="<<kind<<" clocks="<<clocks<<" calls="<<cases<<" steps="<<steps<<'\n'<<std::flush;
 }
 check(nativeAllocations==0,"Native store UI allocated");
 check(std::memcmp(original->memory,native->memory,1<<24)==0,"Store UI final memory differs");
 {
  ks::StateStoreUiCalls capacity;
  for(unsigned i=0;i<16;++i){native->pc=native->makepc(0x482a);native->setSP(0xa18000-i*128);check(capacity.step(*native),"Capacity admission failed");}
  check(capacity.activeCalls()==16,"Fixed call capacity changed");
  native->pc=native->makepc(0x482a);native->setSP(0xa18000-16*128);const auto before=native->cycles;bool rejected=false;
  try{capacity.step(*native);}catch(const std::runtime_error&){rejected=true;}
  check(rejected&&native->cycles==before&&native->getPC()==0x482a&&native->getSP()==0xa18000-16*128,"Overflow mutated CPU state");
  capacity.discardCalls();check(!capacity.activeCalls(),"Cannot discard fixed call contexts");
 }
 std::cout<<"PASS: "<<cases<<" complete store UI calls / "<<steps<<" original boundaries / "<<irqs<<" IRQ points / "<<resets<<" metric resets / "<<nested<<" nested / "<<paired<<" paired publications; Program/Performance commit, store pages, protection and live category/mode, live accesses/clocks/state/full memory exact, zero interpreter/recovery/native new; fixed16-call overflow rejected without mutation\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
