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
    check(a.getPC()==b.getPC(),"state-init call PC projection differs");
    check(a.cycles==b.cycles,"state-init call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"state-init call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("state-init call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"state-init call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"state-init call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original state-init fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original state-init fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original state-init fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("state-init call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2)return 77;check(!ks::controllerReferenceAvailable(),"Service fixture linked recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x494e4954);uint64_t cases=0,steps=0,irqs=0,resets=0;std::array<unsigned,0x4340> first{};
 auto verify=[&](unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,unsigned preferred,unsigned mask){
  shared.stateInitCalls.discardCalls();shared.serviceCycleCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();
  for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());const unsigned stack=odd?0xfff001:0xa18000;a->setSP(stack);b->setSP(stack);
  for(auto* m:{a->memory,b->memory}){for(unsigned n=0;n<256;++n)m[0x168fc+n]=uint8_t(n*17+mask);for(unsigned n=0;n<40;++n)m[0x16782+n]=uint8_t(n+flags);m[0x169ea]=uint8_t(preferred);}
  a->pc=a->makepc(0x41e2);b->pc=b->makepc(0x41e2);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0,dispatches=0;bool opaque=false,injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" flags="<<flags<<" preferred="<<preferred<<" mask="<<mask<<" count="<<count<<'\n';throw;}
   ++steps;if(before<first.size()&&!first[before])first[before]=count+1;return before;};
  do{
   if(opaque){for(unsigned i=0;i<7;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());a->ccr=b->ccr=uint8_t(rng());a->pc=a->makepc(0x4c8);b->pc=b->makepc(0x4c8);opaque=false;}
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count%7==5){shared.resetMetrics();++resets;}
   const unsigned before=step();
   if(ks::StateInitCalls::candidate(before)&&!ks::StateInitCalls::candidate(unsigned(a->getPC()))){++dispatches;opaque=true;}
   check(++count<512,"Native initialization stalled");
  }while(a->getPC()!=0x4338);
  check(a->getSP()==stack&&b->getSP()==stack,"Initialization stack differs");
  step(); // terminal native metadata retires into the actual first service call
  check(!b->interpretedSteps&&!shared.stateInitCalls.partialEntries&&shared.stateInitCalls.completed>=1,"Native initialization used recovery/did not finish");
  check(dispatches==24,"Initialization child count differs");check(std::memcmp(a->memory+0x16780,b->memory+0x16780,0x300)==0,"Initialized state differs");++cases;if(injected)++irqs;
 };
 for(bool clockOnly:{false,true})for(bool odd:{false,true}){
  for(unsigned flags=0;flags<256;++flags)verify(flags,clockOnly,odd,unsigned(-1),flags,flags*7);
  for(unsigned preferred:{0u,1u,127u,128u,255u})for(unsigned mask=0;mask<256;++mask)verify(0xd0,clockOnly,odd,unsigned(-1),preferred,mask);
  first.fill(0);verify(0xd0,clockOnly,odd,unsigned(-1),0,0);const auto selected=first;
  for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::StateInitCalls::candidate(pc))verify(0xe7,clockOnly,odd,selected[pc]-1,1,255);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final full memory differs");
 std::cout<<"PASS: "<<cases<<" native state initialization calls / "<<steps<<" independent original steps; "<<irqs<<" IRQ sites / "<<resets<<" metric resets; all flags/both fetch modes/stack alignments, late patch/mode/clock values, exact ordered state publication/clocks; children intentionally opaque, full boot checks separate; zero recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
