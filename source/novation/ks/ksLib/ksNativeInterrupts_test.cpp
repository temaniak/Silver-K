#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>
namespace { bool track=false; uint64_t allocations=0; }
void* operator new(std::size_t n){if(track)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
using Access=std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>;
using Transfer=std::tuple<bool,uint32_t,uint8_t,uint64_t>;
struct Original final:H8SEmulator{using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
template<class Base>struct Bus final:Base {
 std::vector<Access> events;
 uint8_t read(uint32_t a)override{auto v=this->state->memory[a];events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}
 void write(uint32_t a,uint8_t v)override{this->state->memory[a]=v;events.emplace_back(true,a,v,this->state->cycles,uint32_t(this->state->getPC()));}
};
bool scratch(uint32_t a){return a>=0xfff700&&a<0xfff800;}
std::vector<Transfer> resources(const std::vector<Access>& events,H8SDisassembler* decoder){
 std::vector<Transfer> out;size_t fetchStart=events.size(),fetchBytes=0;
 if(decoder){
  for(size_t i=0;i<events.size();++i){const auto&e=events[i];
   if(!std::get<0>(e)&&!(std::get<4>(e)&1)&&std::get<1>(e)==std::get<4>(e)){
    fetchStart=i;const auto pc=std::get<1>(e);decoder->pc=decoder->makepc(pc);decoder->step();fetchBytes=uint32_t(decoder->getPC())-pc;
    check(fetchBytes>=2&&fetchBytes<=10&&i+fetchBytes<=events.size(),"Original fetch length invalid");
    for(unsigned n=0;n<fetchBytes;++n)check(!std::get<0>(events[i+n])&&std::get<1>(events[i+n])==pc+n,"Original fetch shape invalid");
    break;
   }
  }
  check(fetchBytes,"Original instruction fetch absent");
 }
 for(size_t i=0;i<events.size();++i){const auto&e=events[i];auto a=std::get<1>(e);
  if((i>=fetchStart&&i<fetchStart+fetchBytes)||scratch(a)||(!std::get<0>(e)&&a>=28*4&&a<92*4))continue;
  out.emplace_back(std::get<0>(e),a,std::get<2>(e),std::get<3>(e));
 }
 return out;
}
}
int main(int argc,char**argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Test linked synchronized H8 adapter");ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid ROM");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();
 for(unsigned base:{0u,0xa00000u}){a->loadmem(rom.data().data(),unsigned(rom.data().size()),base);b->loadmem(rom.data().data(),unsigned(rom.data().size()),base);}
 decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);g_dasm=false;
 Bus<H8SDevice>x;Bus<ks::ControllerBus>y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);
 b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;
 ks::LiveMidiController state(*b);ks::RuntimeController runtime(*b,state);std::mt19937 rng(0x4b534952);
 uint64_t cases=0,boundaries=0,nested=0,mutations=0,metricResets=0;
 auto verify=[&](unsigned vector,unsigned producer,unsigned consumer,unsigned status,unsigned flags,unsigned injectAt=~0u,bool mutate=false,unsigned mask=0,unsigned priority=1,unsigned injectedVector=32,unsigned timerVariant=0){
  check(!runtime.activeNativeInterrupts(),"Previous native interrupt did not retire");runtime.resetMetrics();
  std::array<int32_t,8> saved{};for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());
  a->setSP(0xfff800);b->setSP(0xfff800);saved[7]=int32_t(0xfff800);
  for(auto*h:{a.get()}){h->pc=h->makepc(0x4338);h->ccr=uint8_t(flags);h->exr=uint8_t(mask);h->cycles=1000;h->indent=0;h->lastread=h->lastwrite=-1;h->interruptMode2=true;h->pending_irqs=h->pending_irqs_high=0;for(auto&p:h->interruptPriority)p=0;h->interruptPriority[vector]=uint8_t(priority);h->interruptPriority[injectedVector]=6;}
  b->pc=0x4338;b->ccr=uint8_t(flags);b->exr=uint8_t(mask);b->cycles=1000;b->indent=0;b->lastread=b->lastwrite=-1;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[vector]=uint8_t(priority);b->interruptPriority[injectedVector]=6;
  if(vector<64)a->pending_irqs=b->pending_irqs=1ull<<vector;else a->pending_irqs_high=b->pending_irqs_high=1ull<<(vector-64);
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));m[0xffff8c]=uint8_t(status);m[0xffff8a]=0xf0;m[0xffed68]=0x7b;
   m[0xffed6c]=uint8_t(producer>>8);m[0xffed6d]=uint8_t(producer);m[0xffed6e]=uint8_t(consumer>>8);m[0xffed6f]=uint8_t(consumer);
   for(unsigned n=0;n<256;++n)m[0xffed70+n]=uint8_t(n*149+flags);
   m[0xffec14]=timerVariant==2?0:1;m[0xffee70]=timerVariant?128:0;
   m[0xffec10]=m[0xffec11]=m[0xffec12]=0;m[0xffec13]=7;
   m[0xffec23]=timerVariant?1:0;m[0xffee97]=timerVariant?2:0;
   m[0xffff51]=0;m[0xffff50]=m[0xffff53]=m[0xffff5d]=0xff;
   for(unsigned n=0;n<4;++n){m[0xffec16+n]=uint8_t(1u<<n);m[0xffec24+n]=uint8_t(128u>>n);}
  }
  unsigned count=0;bool injected=false;
  do {
   if(count==injectAt){
    if(injectedVector<64)a->pending_irqs=b->pending_irqs=1ull<<injectedVector;
    else a->pending_irqs_high=b->pending_irqs_high=1ull<<(injectedVector-64);
    injected=true;
   }
   if(mutate&&count==5){for(auto*m:{a->memory,b->memory}){m[0xffff8c]^=0xff;m[0xffed6d]=uint8_t(producer+17);m[0xffed6f]=uint8_t(consumer+37);}++mutations;}
   if(count==3){runtime.resetMetrics();++metricResets;}
   x.events.clear();y.events.clear();track=true;runtime.step(*b);track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"vector="<<vector<<" boundary="<<count<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("IRQ phase clocks differ");}
   auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);
   check(std::equal(a->memory+0xffdc00,a->memory+0xfff700,b->memory+0xffdc00),"IRQ evolving resource memory differs");
   if(expected!=actual){std::cerr<<"vector="<<vector<<" boundary="<<count<<" producer="<<producer<<" consumer="<<consumer<<" old/new transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto& rows:{expected,actual}){for(const auto& e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ' ;std::cerr<<'\n';}std::cerr<<" inject="<<injectAt<<" old/new pc="<<std::hex<<a->getPC()<<'/'<<b->getPC()<<std::dec<<'\n';throw std::runtime_error("IRQ observable transfer/value/order/time differs");}
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"IRQ admission/priority differs");
   // With no compatibility child, these fields and bytes never move at all.
   {check(b->pc==0x4338&&b->getSP()==0xfff800&&b->ccr==uint8_t(flags),"Native IRQ used a CPU continuation");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Native IRQ used CPU registers");for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Native IRQ wrote CPU stack");}
   check(++count<8192,"IRQ did not finish");++boundaries;
  }while(a->getPC()!=0x4338||a->indent!=0);
  check(!runtime.activeNativeInterrupts(),"Native IRQ left an active continuation");
  check(runtime.nativeInterruptStarts+runtime.nativeInterruptInitial==runtime.nativeInterruptReturns,"Native IRQ metric reset lost a continuation");
  for(unsigned n=0;n<8;++n)check(a->regs[n].er==b->regs[n].er,"Returned IRQ register differs");
  check(a->ccr==b->ccr&&a->exr==b->exr&&a->indent==b->indent&&a->lastread==b->lastread&&a->lastwrite==b->lastwrite,"Returned IRQ flags/bus history differ");
  check(std::equal(a->memory+0xffdc00,a->memory+0xfff700,b->memory+0xffdc00)&&std::equal(a->memory+0xfff800,a->memory+0xfffc00,b->memory+0xfff800),"IRQ resource memory differs");
  if(injected){
   check(std::equal(a->memory,a->memory+0xfff700,b->memory)
       &&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),
       "Nested IRQ full resource memory differs");
  }
  ++cases;if(injected)++nested;return count;
 };
 for(unsigned flags=0;flags<256;++flags)for(unsigned status=0;status<256;++status)verify(88,0,0,status,flags);
 for(unsigned producer=0;producer<256;++producer)for(unsigned consumer=0;consumer<256;++consumer)verify(90,producer,consumer,(producer*13+consumer)&255,(producer+consumer*149)&255);
 for(unsigned vector:{42u,91u})for(unsigned flags=0;flags<256;++flags)verify(vector,0,0,0,flags);
 for(unsigned vector:{88u,90u,42u,91u})for(unsigned empty:{0u,1u}){
  auto count=verify(vector,empty?17:45,17,0xff,0xa5);
  for(unsigned at=1;at<count;++at)verify(vector,empty?17:45,17,0xff,0xa5,at);
  verify(vector,empty?17:45,17,0x3f,0x57,~0u,true);
 }
 // Native jobs can also preempt one another. Every inner continuation is
 // independent of both the outer task and the remaining CPU frame.
 uint64_t nativeNested=0;
 for(unsigned outer:{88u,90u})for(unsigned inner:{88u,90u,42u,91u})if(outer!=inner){
  auto count=verify(outer,45,17,0xb7,0xd3);
  for(unsigned at=1;at<count;++at){verify(outer,45,17,0xb7,0xd3,at,false,0,1,inner);++nativeNested;}
 }
 // Exercise the whole higher-priority timer, including board scan,
 // clock output, panel decay and the one-shot latch child.
 uint64_t fullTimerNested=0;
 for(unsigned outer:{88u,90u})for(unsigned variant:{1u,2u}){
  auto count=verify(outer,45,17,0xb7,0xd3);
  for(unsigned at=1;at<count;++at){verify(outer,45,17,0xb7,0xd3,at,false,0,1,32,variant);++fullTimerNested;}
 }
 // All admitted masks/priorities, including retained EXR upper bits.
 for(unsigned vector:{88u,90u,42u,91u})for(unsigned priority=1;priority<8;++priority)for(unsigned mask=0;mask<priority;++mask)verify(vector,255,0,0x83,0xec,~0u,false,mask|0xa0,priority);
 check(!allocations&&!b->interpretedSteps,"Native IRQ allocated or called the interpreter");
 std::cout<<"PASS: "<<cases<<" complete native IRQs; "<<boundaries<<" independent H8 boundaries; "<<nested<<" total nested cases ("<<nativeNested<<" native/native, "<<fullTimerNested<<" full TPU0); "<<mutations<<" live mutations; "<<metricResets<<" metric resets; zero allocations/interpreter/CPU stack writes in closed jobs\n";
 return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
