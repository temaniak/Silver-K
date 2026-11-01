#include "ksRuntimeController.h"
#include "ksPanelAdcIrqCalls.h"
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
    check(a.getPC()==b.getPC(),"ADC IRQ call PC projection differs");
    check(a.cycles==b.cycles,"ADC IRQ call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"ADC IRQ call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("ADC IRQ call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"ADC IRQ call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"ADC IRQ call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original ADC IRQ fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original ADC IRQ fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original ADC IRQ fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("ADC IRQ call data/stack/value/order/clock/in-flight context differs");
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
 ks::LiveMidiController shared(*native);ks::RuntimeController runtime(*native,shared);auto& calls=shared.panelAdcIrqCalls;
 OriginalBus oldBus;NativeBus newBus;oldBus.events.reserve(256);newBus.events.reserve(256);
 original->memmap(&oldBus,0,1<<24);native->memmap(&newBus,0,1<<24);native->interpreterAllowed=false;
 std::mt19937 rng(0x52494e47);uint64_t cases=0,steps=0,irqs=0,resets=0,nested=0;
 constexpr std::array<unsigned,1> roots{{0x165c}};
 std::array<unsigned,0x2300> first{};
 auto verify=[&](unsigned kind,unsigned a,unsigned b,unsigned flags,bool clocksOnly,unsigned irqAt,bool mutate,bool nest){
  calls.discardCalls();shared.panelAdcSamplesCalls.discardCalls();shared.panelAdcScaleCalls.discardCalls();shared.panelInputCalls.discardCalls();shared.panelMatrixCalls.discardCalls();shared.pedalSenseCalls.discardCalls();shared.panelEncoderCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();
  for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int32_t(rng());
  const bool irqMode2=!(b&512);
  const unsigned stack=(a&1)?0xfff001u:0xa18000u;
  auto byte=[&](unsigned address,uint8_t value){original->memory[address]=native->memory[address]=value;};
  auto word=[&](unsigned address,uint16_t value){byte(address,uint8_t(value>>8));byte(address+1,uint8_t(value));};
  byte(112,0);byte(113,0);byte(114,1);byte(115,0x70);
  const auto group=uint8_t(b),remaining=uint8_t(kind==0?0:kind==1?1:a);
  for(unsigned channel=0;channel<256;++channel){byte(0xb918+channel,uint8_t(channel));byte(0xb93c+channel,uint8_t(channel&1));}
  for(unsigned channel=0;channel<4;++channel){const auto current=uint16_t(0x4000+channel*100);word(0x15652+uint8_t(group<<1)+2*channel,current);word(0xffff90+2*channel,current);word(0x1560a+uint8_t(group<<1)+2*channel,uint16_t(current+a));byte(0x1569a+uint8_t(group<<1)+2*channel,uint8_t((b>>8)&1));}
  word(0x15406,0);word(0x15408,0);byte(0x156e2,group);byte(0x156e3,remaining);byte(0x156e4,255);byte(0xffff98,uint8_t(a));
  auto frame=[&](unsigned at){if(irqMode2){word(at,0);at+=2;}byte(at,uint8_t(flags));byte(at+1,0);byte(at+2,0x43);byte(at+3,0x38);};frame(stack);
  original->pc=original->makepc(roots[0]);native->pc=native->makepc(roots[0]);original->setSP(stack);native->setSP(stack);
  original->ccr=native->ccr=uint8_t(flags);original->exr=native->exr=0;original->cycles=native->cycles=1000;
  original->lastread=native->lastread=int(rng());original->lastwrite=native->lastwrite=int(rng());original->indent=native->indent=0;
  original->pending_irqs=native->pending_irqs=original->pending_irqs_high=native->pending_irqs_high=0;
  original->interruptMode2=native->interruptMode2=irqMode2;
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
    if(pc==0x166c)original->lastwrite=native->lastwrite=0x156e2;
    if(pc==0x165e)byte(0x156e3,uint8_t((remaining+count)&255));
    if(pc==0x1672||pc==0x16e4)byte(0xffff98,uint8_t(a+count));
    if(pc==0x1700)byte(0x156e2,uint8_t((group+count*4)%36));
    if(pc==0x171c)word((uint32_t(original->regs[0].er)+72)&0xffffff,uint16_t(a+count*149));
    if(pc==0x1720||pc==0x1756)word(uint32_t(original->regs[0].er)&0xffffff,uint16_t(a+count*971));
    if(pc==0x1726)byte((uint32_t(original->regs[0].er)+144)&0xffffff,uint8_t(count&1));
    if(pc==0x175c)byte(0x156e2,uint8_t(count&1?32:28));
    if(pc==0x1792)byte(0x156e4,uint8_t(count));
    if(pc==0x1818)byte(0x156e2,uint8_t(b+count));
   }
   step();check(++count<4000,"ADC IRQ stalled");
   if(nest&&count==3){
    std::array<int32_t,8> oldRegs{},newRegs{};for(unsigned i=0;i<8;++i){oldRegs[i]=original->regs[i].er;newRegs[i]=native->regs[i].er;}
    const auto oldPC=original->pc;const auto newPC=native->pc;const auto oldFlags=original->ccr,newFlags=native->ccr;
    const auto oldMask=original->exr,newMask=native->exr;const auto oldDepth=original->indent,newDepth=native->indent;
    const unsigned child=stack-512;frame(child);original->setSP(child);native->setSP(child);
    original->pc=original->makepc(roots[0]);native->pc=native->makepc(roots[0]);
    unsigned childSteps=0;do{step();check(++childSteps<4000,"Nested ADC IRQ stalled");}while(original->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){original->regs[i].er=oldRegs[i];native->regs[i].er=newRegs[i];}
    original->pc=oldPC;native->pc=newPC;original->ccr=oldFlags;native->ccr=newFlags;
    original->exr=oldMask;native->exr=newMask;original->indent=oldDepth;native->indent=newDepth;++nested;
   }
  }while(original->getPC()!=0x4338);
  check(native->getSP()==stack+(irqMode2?6:4),"ADC IRQ return frame");
  check(!native->interpretedSteps&&!calls.partialEntries,"ADC IRQ used recovery/interpreter");
  check(calls.started+calls.initialActive==calls.completed+calls.activeCalls()&&!calls.activeCalls(),"ADC IRQ metadata lifetime");
  ++cases;if(injected)++irqs;
 };
 for(bool clocks:{false,true})for(unsigned mode:{0u,512u}){
  for(unsigned delta=0;delta<65536;++delta)for(unsigned armed:{0u,256u})verify(0,delta,mode|armed,delta&255,clocks,unsigned(-1),false,false);
  for(unsigned remaining=1;remaining<256;++remaining)verify(remaining==1?1:2,remaining,mode|32,remaining&255,clocks,unsigned(-1),false,false);
  for(unsigned group:{0u,28u,32u,36u,72u,127u,128u,252u,255u})for(unsigned delta:{0u,1u,511u,512u,2047u,2048u,32767u,32768u,65535u}){
   first.fill(0);verify(0,delta,mode|group|256,0x50,clocks,unsigned(-1),false,false);const auto observed=first;
   for(unsigned pc=0;pc<observed.size();++pc)if(observed[pc]&&ks::PanelAdcIrqCalls::candidate(pc))verify(0,delta,mode|group|256,0x50,clocks,observed[pc]-1,true,false);
   verify(0,delta,mode|group|256,0xa5,clocks,unsigned(-1),true,true);
  }
 }
 check(nativeAllocations==0,"Native ADC IRQ allocated");
 check(std::memcmp(original->memory,native->memory,1<<24)==0,"ADC IRQ final memory differs");
 {
  ks::PanelAdcIrqCalls capacity;
  for(unsigned i=0;i<16;++i){native->pc=native->makepc(0x165c);native->setSP(0xa18000-i*128);check(capacity.step(*native),"Capacity admission failed");}
  check(capacity.activeCalls()==16,"Fixed call capacity changed");
  native->pc=native->makepc(0x165c);native->setSP(0xa18000-16*128);const auto before=native->cycles;bool rejected=false;
  try{capacity.step(*native);}catch(const std::runtime_error&){rejected=true;}
  check(rejected&&native->cycles==before&&native->getPC()==0x165c&&native->getSP()==0xa18000-16*128,"Overflow mutated CPU state");
  capacity.discardCalls();check(!capacity.activeCalls(),"Cannot discard fixed call contexts");
 }
 std::cout<<"PASS: "<<cases<<" complete ADC IRQ calls / "<<steps<<" original boundaries / "<<irqs<<" IRQ points / "<<resets<<" metric resets / "<<nested<<" nested; all signed/wrapping threshold differences, settle counts, both IRQ frame modes, ordered samples/events, late observations and group edges, live accesses/clocks/state/full memory exact, zero interpreter/recovery/native new; fixed16-call overflow rejected without mutation\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
