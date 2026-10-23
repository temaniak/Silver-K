#include "ksRuntimeController.h"
#include "ksStateScreenCalls.h"
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
  const auto value=this->state->memory[address];
  events.emplace_back(false,address,value,this->state->cycles,uint32_t(this->state->getPC()));return value;
 }
 void write(uint32_t address,uint8_t value)override{
  events.emplace_back(true,address,value,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[address]=value;
 }
};
using OriginalBus=MockBus<H8SDevice>;using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"state screen call PC projection differs");
    check(a.cycles==b.cycles,"state screen call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"state screen call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("state screen call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"state screen call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"state screen call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original state screen fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original state screen fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original state screen fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("state screen call data/stack/value/order/clock/in-flight context differs");
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
 ks::LiveMidiController shared(*native);ks::RuntimeController runtime(*native,shared);auto& calls=shared.stateScreenCalls;
 OriginalBus oldBus;NativeBus newBus;oldBus.events.reserve(256);newBus.events.reserve(256);
 original->memmap(&oldBus,0,1<<24);native->memmap(&newBus,0,1<<24);native->interpreterAllowed=false;
 std::mt19937 rng(0x52494e47);uint64_t cases=0,steps=0,irqs=0,resets=0,nested=0;
 constexpr std::array<unsigned,8> roots{{0x582a,0x58ae,0x58ce,0x58e4,0x5900,0x590a,0x5932,0x5976}};
 std::array<unsigned,0x5b00> first{};
 std::vector<unsigned> descriptors{0x10517,0x10569};for(unsigned n=0;n<64;++n){descriptors.push_back(0x30000 + n*0x2000);descriptors.push_back(0x130000 + n*0x2000);}
 for(auto head:descriptors)for(unsigned page=0;page<256;++page){const unsigned record=head+2+page*20;for(unsigned i=0;i<16;++i)original->memory[record+i]=native->memory[record+i]=i%3?uint8_t(65+i):255;for(unsigned i=16;i<20;++i)original->memory[record+i]=native->memory[record+i]=i==16?3:0;}

 auto verify=[&](unsigned kind,unsigned a,unsigned b,unsigned flags,bool clocksOnly,unsigned irqAt,bool mutate,bool nest){
  calls.discardCalls();shared.panelInputCalls.discardCalls();shared.panelMatrixCalls.discardCalls();shared.pedalSenseCalls.discardCalls();shared.panelEncoderCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();
  for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int32_t(rng());
  const unsigned stack=(a&1)?0xfff001u:0xa18000u;
  auto byte=[&](unsigned address,uint8_t value){original->memory[address]=native->memory[address]=value;};
  auto word=[&](unsigned address,uint16_t value){byte(address,uint8_t(value>>8));byte(address+1,uint8_t(value));};
  byte(112,0);byte(113,0);byte(114,1);byte(115,0x70);
  auto dword=[&](unsigned address,uint32_t value){word(address,uint16_t(value>>16));word(address+2,uint16_t(value));};
  for(unsigned n=0;n<64;++n)dword(0x101ae + n*4,n==12?0x10517:0x30000 + n*0x2000);
  for(unsigned n=0;n<64;++n)dword(0x101e6 + n*4,0x130000 + n*0x2000);
  for(auto head:descriptors){byte(head,(a&0x1000000)&&head<0x130000?255:uint8_t(b));byte(head+1,uint8_t(b>>8));}
  byte(0x167ba,uint8_t(a>>8));byte(0xffec14,uint8_t(a>>16));byte(0x16785,uint8_t(a));byte(0x16787,uint8_t(b>>8));byte(0x169f7,uint8_t(b>>24));byte(0x16784,uint8_t(b>>24));
  dword(0x167bc,0x280000);byte(0x280000 + 226,uint8_t(b>>16));dword(0x167f0,0x30002 + uint8_t(b>>8)*20);dword(0x167a6,0);byte(0x10d6e + 3,0);byte(0x117a2 + 3,0x7e);
  original->regs[0].rl=native->regs[0].rl=int8_t(a);original->regs[6].rl=native->regs[6].rl=int8_t(a);
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
   try{trackAllocations=true;runtime.step(*native);trackAllocations=false;original->step();same(*original,*native,oldBus,newBus,clocksOnly);}
   catch(const std::exception&){trackAllocations=false;std::cerr<<"before="<<std::hex<<before<<std::dec<<" kind="<<kind<<" a="<<a<<" b="<<b<<" flags="<<flags<<" clocks="<<clocksOnly<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;
  };
  do{
   if(count==irqAt){original->pending_irqs=native->pending_irqs=1ull<<28;injected=true;}
   if(count%5==2){shared.resetMetrics();++resets;}
   if(mutate){
    const auto pc=unsigned(original->getPC());
    if(pc==0x582e||pc==0x588e)byte(0x167ba,uint8_t(count));
    if(pc==0x584a||pc==0x590c)byte(0xffec14,uint8_t(count&1));
    if(pc==0x58c0)byte(0x169f7,uint8_t(count&1));
    if(pc==0x5914)byte(0x16785,9);
    if(pc==0x591a)byte(0x16787,2);
    if(pc==0x4efe)byte(0x16784,uint8_t(count&1));
    if(pc==0x586e)byte(0x280000 + 226,uint8_t(b>>16));
    if(pc==0x589c||pc==0x58aa)byte(unsigned(original->regs[0].er)+(pc==0x589c?1:0),uint8_t(b>>8));
    if(pc==0x5986)byte(unsigned(original->regs[0].er),uint8_t(count%5?65:255));
   }
   step();check(++count<30000,"state screen stalled");
   if(nest&&count==3){
    std::array<int32_t,8> oldRegs{},newRegs{};for(unsigned i=0;i<8;++i){oldRegs[i]=original->regs[i].er;newRegs[i]=native->regs[i].er;}
    const auto oldPC=original->pc;const auto newPC=native->pc;const auto oldFlags=original->ccr,newFlags=native->ccr;
    const auto oldMask=original->exr,newMask=native->exr;const auto oldDepth=original->indent,newDepth=native->indent;
    const unsigned child=stack-512;frame(child);original->setSP(child);native->setSP(child);
    original->pc=original->makepc(roots[kind]);native->pc=native->makepc(roots[kind]);
    unsigned childSteps=0;do{step();check(++childSteps<30000,"Nested state screen stalled");}while(original->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){original->regs[i].er=oldRegs[i];native->regs[i].er=newRegs[i];}
    original->pc=oldPC;native->pc=newPC;original->ccr=oldFlags;native->ccr=newFlags;
    original->exr=oldMask;native->exr=newMask;original->indent=oldDepth;native->indent=newDepth;++nested;
   }
  }while(original->getPC()!=0x4338);
  check(native->getSP()==stack+4,"state screen return frame");
  check(!native->interpretedSteps&&!calls.partialEntries,"state screen used recovery/interpreter");
  check(calls.started+calls.initialActive==calls.completed+calls.activeCalls()&&!calls.activeCalls(),"state screen metadata lifetime");
  ++cases;if(injected)++irqs;
 };
 for(bool clocks:{false,true}){
  for(unsigned index=0;index<256;++index)for(unsigned mode:{0u,1u})for(unsigned hardware:{0u,1u})for(unsigned variant:{0u,6u,255u})verify(0,index|(mode<<8)|(hardware<<16)|0x1000000u,4u|(255u<<8)|(variant<<16),index&255,clocks,unsigned(-1),false,false);
  for(unsigned pair=0;pair<65536;++pair)verify(0,12u|((pair&1)<<8)|((pair&2)<<15),pair,pair&255,clocks,unsigned(-1),false,false);
  for(unsigned kind=1;kind<8;++kind)for(unsigned page:{0u,1u,2u,3u,4u,127u,128u,255u})for(unsigned n:{0u,1u,4u,255u})for(unsigned option:{0u,1u})verify(kind,9u|(option<<8),n|(page<<8)|(6u<<16)|(option<<24),0x50,clocks,unsigned(-1),false,false);
  for(unsigned kind=0;kind<8;++kind)for(unsigned a:{9u,0x101000cu,0x100010du})for(unsigned b:{0x060204u,0x01060304u}){
   first.fill(0);verify(kind,a,b,0x50,clocks,unsigned(-1),false,false);const auto observed=first;
   for(unsigned pc=0;pc<observed.size();++pc)if(observed[pc]&&ks::StateScreenCalls::candidate(pc))verify(kind,a,b,0x53,clocks,observed[pc]-1,true,false);
   verify(kind,a,b,0xa5,clocks,unsigned(-1),true,true);
  }
 }
 check(nativeAllocations==0,"Native state screen allocated");
 check(std::memcmp(original->memory,native->memory,1<<24)==0,"state screen final memory differs");
 {
  ks::StateScreenCalls capacity;
  for(unsigned i=0;i<16;++i){native->pc=native->makepc(0x582a);native->setSP(0xa18000-i*128);check(capacity.step(*native),"Capacity admission failed");}
  check(capacity.activeCalls()==16,"Fixed call capacity changed");
  native->pc=native->makepc(0x582a);native->setSP(0xa18000-16*128);const auto before=native->cycles;bool rejected=false;
  try{capacity.step(*native);}catch(const std::runtime_error&){rejected=true;}
  check(rejected&&native->cycles==before&&native->getPC()==0x582a&&native->getSP()==0xa18000-16*128,"Overflow mutated CPU state");
  capacity.discardCalls();check(!capacity.activeCalls(),"Cannot discard fixed call contexts");
 }
 std::cout<<"PASS: "<<cases<<" complete state screen calls / "<<steps<<" original boundaries / "<<irqs<<" IRQ points / "<<resets<<" metric resets / "<<nested<<" nested; data-only descriptors (stock instruction bytes), all page/count pairs, selector/variant wrap and dynamic clamp, navigation/gate/labels and live observations, live accesses/clocks/state/full memory exact, zero interpreter/recovery/native new; fixed16-call overflow rejected without mutation\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
