#include "ksRuntimeController.h"
#include "ksPanelInputCalls.h"
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
    check(a.getPC()==b.getPC(),"Panel input call PC projection differs");
    check(a.cycles==b.cycles,"Panel input call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"Panel input call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("Panel input call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"Panel input call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"Panel input call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original Panel input fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original Panel input fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original Panel input fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("Panel input call data/stack/value/order/clock/in-flight context differs");
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
 ks::LiveMidiController shared(*native);ks::RuntimeController runtime(*native,shared);auto& calls=shared.panelInputCalls;
 OriginalBus oldBus;NativeBus newBus;oldBus.events.reserve(256);newBus.events.reserve(256);
 original->memmap(&oldBus,0,1<<24);native->memmap(&newBus,0,1<<24);native->interpreterAllowed=false;
 std::mt19937 rng(0x52494e47);uint64_t cases=0,steps=0,irqs=0,resets=0,nested=0;
 constexpr std::array<unsigned,7> roots{{0x13fe,0x1412,0x14e4,0x1578,0x15da,0x21e2,0x1848}};
 std::array<unsigned,0x2300> first{};
 auto verify=[&](unsigned kind,unsigned a,unsigned b,unsigned flags,bool clocksOnly,unsigned irqAt,bool mutate,bool nest){
  calls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();
  for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int32_t(rng());
  const unsigned stack=(a&1)?0xfff001u:0xa18000u;
  auto byte=[&](unsigned address,uint8_t value){original->memory[address]=native->memory[address]=value;};
  auto word=[&](unsigned address,uint16_t value){byte(address,uint8_t(value>>8));byte(address+1,uint8_t(value));};
  byte(112,0);byte(113,0);byte(114,1);byte(115,0x70);
  const unsigned producer=kind<3?0x1537c:0x15406,consumer=producer+2,buffer=producer+4;
  const uint16_t index=a==255?0xffff:a==254?0xfffe:uint16_t(a*2);
  word(producer,b%8?uint16_t(index+2):index);word(consumer,index);
  const uint16_t event=uint16_t((b<<8)|a);word(buffer+index,event);word(buffer+uint16_t(index+2),uint16_t(event^0x55aa));
  byte(0xb8e8+a,uint8_t(b));byte(0xb918+a,uint8_t(b));
  original->regs[6].r=native->regs[6].r=int16_t(event);original->regs[1].r=native->regs[1].r=int16_t(event);
  word(0x158ec,0x8040);word(0x158ee,0x8080);word(0x159ec,0x60a0);word(0x159ee,0x60a0);
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
    if(pc==0x14e8||pc==0x1854)byte((kind<3?0xb8e8:0xb918)+a,uint8_t(b^0x80));
    if(pc==0x141c||pc==0x15e4||pc==0x21ec)word(producer,uint16_t(index+(b&1?0:2)));
    if(pc==0x144a||pc==0x1610||pc==0x2218){word(buffer+index,uint16_t(event^0xa55a));word(consumer,uint16_t(index^0x7e));}
   }
   step();check(++count<2000,"Panel input stalled");
   if(nest&&count==3){
    std::array<int32_t,8> oldRegs{},newRegs{};for(unsigned i=0;i<8;++i){oldRegs[i]=original->regs[i].er;newRegs[i]=native->regs[i].er;}
    const auto oldPC=original->pc;const auto newPC=native->pc;const auto oldFlags=original->ccr,newFlags=native->ccr;
    const auto oldMask=original->exr,newMask=native->exr;const auto oldDepth=original->indent,newDepth=native->indent;
    const unsigned child=stack-512;frame(child);original->setSP(child);native->setSP(child);
    original->pc=original->makepc(roots[kind]);native->pc=native->makepc(roots[kind]);
    unsigned childSteps=0;do{step();check(++childSteps<2000,"Nested panel input stalled");}while(original->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){original->regs[i].er=oldRegs[i];native->regs[i].er=newRegs[i];}
    original->pc=oldPC;native->pc=newPC;original->ccr=oldFlags;native->ccr=newFlags;
    original->exr=oldMask;native->exr=newMask;original->indent=oldDepth;native->indent=newDepth;++nested;
   }
  }while(original->getPC()!=0x4338);
  check(native->getSP()==stack+4,"Panel input return frame");
  check(!native->interpretedSteps&&!calls.partialEntries,"Panel input used recovery/interpreter");
  check(calls.started+calls.initialActive==calls.completed+calls.activeCalls()&&!calls.activeCalls(),"Panel input metadata lifetime");
  ++cases;if(injected)++irqs;
 };
 for(bool clocks:{false,true})for(unsigned kind=0;kind<7;++kind){
  const bool reset=kind==0||kind==3;
  for(unsigned a=0;a<256;++a)for(unsigned b=0;b<(reset?1u:256u);++b)verify(kind,a,b,(a+b*149)&255,clocks,unsigned(-1),false,false);
  for(unsigned a:{0u,1u,32u,34u,35u,63u,127u,254u,255u})for(unsigned b:{0u,1u,127u,128u,255u}){
   first.fill(0);verify(kind,a,b,0x50,clocks,unsigned(-1),false,false);const auto observed=first;
   for(unsigned pc=0;pc<observed.size();++pc)if(observed[pc]&&ks::PanelInputCalls::candidate(pc))verify(kind,a,b,0x50,clocks,observed[pc]-1,true,false);
   verify(kind,a,b,0xa5,clocks,unsigned(-1),true,true);
  }
 }
 check(nativeAllocations==0,"Native panel input allocated");
 check(std::memcmp(original->memory,native->memory,1<<24)==0,"Panel input final memory differs");
 {
  ks::PanelInputCalls capacity;
  for(unsigned i=0;i<16;++i){native->pc=native->makepc(0x13fe);native->setSP(0xa18000-i*128);check(capacity.step(*native),"Capacity admission failed");}
  check(capacity.activeCalls()==16,"Fixed call capacity changed");
  native->pc=native->makepc(0x13fe);native->setSP(0xa18000-16*128);const auto before=native->cycles;bool rejected=false;
  try{capacity.step(*native);}catch(const std::runtime_error&){rejected=true;}
  check(rejected&&native->cycles==before&&native->getPC()==0x13fe&&native->getSP()==0xa18000-16*128,"Overflow mutated CPU state");
  capacity.discardCalls();check(!capacity.activeCalls(),"Cannot discard fixed call contexts");
 }
 std::cout<<"PASS: "<<cases<<" complete panel input calls / "<<steps<<" original boundaries / "<<irqs<<" IRQ points / "<<resets<<" metric resets / "<<nested<<" nested; empty/wrap/full-byte events and mappings, live accesses/clocks/state/full memory exact, zero interpreter/recovery/native new; fixed16-call overflow rejected without mutation\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
