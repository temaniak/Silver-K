#include "ksRuntimeController.h"
#include "ksTimerServiceCalls.h"
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
    check(a.getPC()==b.getPC(),"Timer service call PC projection differs");
    check(a.cycles==b.cycles,"Timer service call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"Timer service call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("Timer service call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"Timer service call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"Timer service call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original Timer service fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original Timer service fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original Timer service fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("Timer service call data/stack/value/order/clock/in-flight context differs");
    }
}
}

int main(int argc,char** argv){try{
 if(argc!=2)return 77;
 check(!ks::controllerReferenceAvailable(),"Independent fixture linked synchronized H8 factory");
 ks::Rom os(argv[1]);check(os.isValid(),"Invalid ROM");
 auto original=std::make_unique<Original>();auto native=std::make_unique<ks::ControllerCpu>();
 lengthOracle=std::make_unique<H8SDisassembler>();
 lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){
  original->loadmem(os.data().data(),unsigned(os.data().size()),base);
  native->loadmem(os.data().data(),unsigned(os.data().size()),base);
 }
 ks::LiveMidiController shared(*native);ks::RuntimeController runtime(*native,shared);
 auto& calls=shared.timerServiceCalls;
 OriginalBus oldBus;NativeBus newBus;oldBus.events.reserve(128);newBus.events.reserve(128);
 original->memmap(&oldBus,0,1<<24);native->memmap(&newBus,0,1<<24);native->interpreterAllowed=false;
 std::mt19937 rng(0x54494d45);uint64_t cases=0,steps=0,irqs=0,resets=0,nested=0;
 constexpr std::array<unsigned,3> roots{{0x12b0,0x12fa,0x27be}};
 std::array<unsigned,0x3000> first{};
 auto verify=[&](unsigned kind,unsigned seed,uint32_t phase,uint32_t increment,unsigned flags,
                 bool clocksOnly,bool extended,unsigned irqAt,bool mutate,bool nest){
  calls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();
  shared.panelDecayCalls.discardCalls();shared.serialTransmitCalls.discardCalls();shared.resetMetrics();
  for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int32_t(rng());
  const unsigned stack=(seed&1)?0xfff001u:0xa18000u;
  auto word=[&](uint32_t address,uint32_t value,unsigned bytes){for(auto* m:{original->memory,native->memory})for(unsigned n=0;n<bytes;++n)m[(address+n)&0xffffff]=uint8_t(value>>((bytes-n-1)*8));};
  word(112,0x170,4);word(0xffec14,seed&1,1);word(0xffffd5,seed^0x3c,1);
  word(0xffee70,seed,1);word(0xffec10,phase,4);
  word(0xffee7a,phase,4);word(0xffee7e,increment,4);word(0xffee78,seed&1?65535:seed*257,2);
  word(0xffee82,~phase,4);word(0xffee86,increment^0x80000000u,4);
  word(0xffee97,seed,1);word(0xffee9a,seed,1);
  word(0xffff55,255,1);word(0xffff54,255,1);word(0xffff59,255,1);word(0xffff50,255,1);
  for(unsigned n=0;n<128;++n)word(0xb960+n,255,1);
  word(0xffff8a,0x70,1);word(0xffff8c,0x84,1);
  word(0xffec64,0,2);word(0xffec66,0,2);word(0xffed6c,0,2);word(0xffed6e,0,2);
  word(0x400004,7,1);word(0x600004,7,1);
  auto frame=[&](unsigned at){
   if(kind==0&&extended){word(at,0x300,2);word(at+2,((flags^0xa5u)<<24)|0x4338u,4);}
   else word(at,((flags^0xa5u)<<24)|0x4338u,4);
  };
  frame(stack);
  for(auto* h:{static_cast<H8SEmulator*>(original.get())}){
   h->pc=h->makepc(roots[kind]);h->setSP(stack);h->ccr=uint8_t(flags);h->exr=2;h->cycles=1000;
   h->pending_irqs=h->pending_irqs_high=0;h->interruptMode2=extended;
   for(auto& value:h->interruptPriority)value=0;h->interruptPriority[28]=7;
  }
  native->pc=native->makepc(roots[kind]);native->setSP(stack);native->ccr=uint8_t(flags);native->exr=2;native->cycles=1000;
  native->pending_irqs=native->pending_irqs_high=0;native->interruptMode2=extended;
  for(auto& value:native->interruptPriority)value=0;native->interruptPriority[28]=7;
  original->lastread=native->lastread=int(rng());original->lastwrite=native->lastwrite=int(rng());
  original->indent=native->indent=0;native->nativeFetchClocksOnly=clocksOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{
   const auto before=unsigned(original->getPC());oldBus.events.clear();newBus.events.clear();
   try{trackAllocations=true;runtime.step(*native);trackAllocations=false;original->step();same(*original,*native,oldBus,newBus,clocksOnly);}
   catch(const std::exception&){trackAllocations=false;std::cerr<<"before="<<std::hex<<before<<" phase="<<phase<<" increment="<<increment<<std::dec<<" kind="<<kind<<" seed="<<seed<<" flags="<<flags<<" clocks="<<clocksOnly<<" mode2="<<extended<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;
  };
  do{
   if(count==irqAt){original->pending_irqs=native->pending_irqs=1ull<<28;injected=true;}
   if(count%19==7){shared.resetMetrics();++resets;}
   if(mutate){
    const unsigned pc=unsigned(original->getPC());
    if(pc==0x12c6)word(0xffee70,seed+count*13,1);
    if(pc==0x12da)word(0xffec10,phase+count*17,4);
    if(pc==0x27e2||pc==0x281e)word(0xffee97,seed+count*23,1);
    if(pc==0x2824)word(0xffee9a,seed+count*29,1);
   }
   step();check(++count<20000,"Timer call stalled");
   if(nest&&count==5){
    std::array<int32_t,8> oldRegs{},newRegs{};for(unsigned i=0;i<8;++i){oldRegs[i]=original->regs[i].er;newRegs[i]=native->regs[i].er;}
    const auto oldPC=original->pc;const auto newPC=native->pc;const auto oldFlags=original->ccr,newFlags=native->ccr;
    const auto oldMask=original->exr,newMask=native->exr;const auto oldDepth=original->indent,newDepth=native->indent;
    const unsigned child=stack-512;frame(child);original->setSP(child);native->setSP(child);
    original->pc=original->makepc(roots[kind]);native->pc=native->makepc(roots[kind]);
    unsigned childSteps=0;do{step();check(++childSteps<20000,"Nested timer stalled");}while(original->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){original->regs[i].er=oldRegs[i];native->regs[i].er=newRegs[i];}
    original->pc=oldPC;native->pc=newPC;original->ccr=oldFlags;native->ccr=newFlags;
    original->exr=oldMask;native->exr=newMask;original->indent=oldDepth;native->indent=newDepth;++nested;
   }
  }while(original->getPC()!=0x4338);
  check(native->getSP()==stack+(kind==0&&extended?6:4),"Timer return frame");
  check(!native->interpretedSteps&&!calls.partialEntries,"Timer used recovery/interpreter");
  check(calls.started+calls.initialActive==calls.completed+calls.activeCalls()&&!calls.activeCalls(),"Timer call metadata lifetime");
  ++cases;if(injected)++irqs;
 };
 for(bool clocks:{false,true})for(bool extended:{false,true})for(unsigned kind=0;kind<3;++kind){
  for(unsigned seed=0;seed<256;++seed){
   const uint32_t phase=seed&1?0xffffffffu:uint32_t(seed)*0x01010101u;
   const uint32_t increment=seed&2?1u:0x80000001u;
   verify(kind,seed,phase,increment,seed,clocks,extended,unsigned(-1),false,false);
   verify(kind,seed,phase,increment,seed^0xa5,clocks,extended,unsigned(-1),true,false);
  }
  for(unsigned seed:{0u,1u,127u,128u,255u}){
   first.fill(0);verify(kind,seed,0xffffffffu,1,0x50,clocks,extended,unsigned(-1),false,false);const auto observed=first;
   for(unsigned pc=0;pc<observed.size();++pc)if(observed[pc]&&ks::TimerServiceCalls::candidate(pc))verify(kind,seed,0xffffffffu,1,0x50,clocks,extended,observed[pc]-1,true,false);
   verify(kind,seed,0xffffffffu,1,0x50,clocks,extended,unsigned(-1),true,true);
  }
 }
 check(nativeAllocations==0,"Native timer steps allocated");
 check(std::memcmp(original->memory,native->memory,1<<24)==0,"Timer final full memory differs");
 std::cout<<"PASS: "<<cases<<" complete timer/clock calls / "<<steps<<" original boundaries / "<<irqs<<" selected IRQ points / "<<resets<<" metric resets / "<<nested<<" nested calls; exact live accesses/order/clocks/registers/flags/full memory, both RTE modes, zero interpreter/recovery/native new\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
