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
 uint64_t cases=0,boundaries=0,nested=0,mutations=0,metricResets=0,liveCases=0;
 struct Config{unsigned producer=45,consumer=17,data=0xf8,status=0xf7,source=0,active=1,flags=0xa4,
  pulseCount=0xffff,pulses=0x1234,fraction=0x5678,samples=0,baseline=0,counter=20000,
  estimate=0x12345678,gain=0x2000,integral=0,sum=0;bool live=false;}cfg;
 uint32_t components=0;
 auto verify=[&](unsigned vector,unsigned producer,unsigned consumer,unsigned status,unsigned flags,unsigned injectAt=~0u,bool mutate=false,unsigned mask=0,unsigned priority=1,unsigned injectedVector=32,unsigned timerVariant=0){
  check(!runtime.activeNativeInterrupts(),"Previous native interrupt did not retire");runtime.resetMetrics();
  std::array<int32_t,8> saved{};for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());
  a->setSP(0xfff800);b->setSP(0xfff800);saved[7]=int32_t(0xfff800);
  for(auto*h:{a.get()}){h->pc=h->makepc(0x4338);h->ccr=uint8_t(flags);h->exr=uint8_t(mask);h->cycles=1000;h->indent=0;h->lastread=h->lastwrite=-1;h->interruptMode2=true;h->pending_irqs=h->pending_irqs_high=0;for(auto&p:h->interruptPriority)p=0;h->interruptPriority[vector]=uint8_t(priority);h->interruptPriority[injectedVector]=uint8_t(6);}
  b->pc=0x4338;b->ccr=uint8_t(flags);b->exr=uint8_t(mask);b->cycles=1000;b->indent=0;b->lastread=b->lastwrite=-1;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[vector]=uint8_t(priority);b->interruptPriority[injectedVector]=uint8_t(6);
  if(vector<64)a->pending_irqs=b->pending_irqs=1ull<<vector;else a->pending_irqs_high=b->pending_irqs_high=1ull<<(vector-64);
  for(auto*m:{a->memory,b->memory}){
   const auto byte=[&](uint32_t at,uint8_t value){m[at]=value;};
   const auto word=[&](uint32_t at,uint16_t value){byte(at,uint8_t(value>>8));byte(at+1,uint8_t(value));};
   const auto wide=[&](uint32_t at,uint32_t value){word(at,uint16_t(value>>16));word(at+2,uint16_t(value));};
   std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));
   byte(0xffff8c,uint8_t(cfg.status));byte(0xffff8d,uint8_t(cfg.data));byte(0xffed68,0x7b);
   word(0xffec64,uint16_t(cfg.producer));word(0xffec66,uint16_t(cfg.consumer));
   for(unsigned n=0;n<256;++n)byte(0xffec68+n,uint8_t(n*149+flags));
   byte(0xffee9a,uint8_t(cfg.source));byte(0xffee96,uint8_t(cfg.active));byte(0xffee97,uint8_t(cfg.flags));
   word(0xffee78,uint16_t(cfg.pulses));word(0xffee7a,uint16_t(cfg.fraction));
   word(0xffeea8,uint16_t(cfg.pulseCount));word(0xffee9c,uint16_t(cfg.samples));word(0xffee9e,uint16_t(cfg.baseline));
   wide(0xffeea0,cfg.estimate);word(0xffeea4,uint16_t(cfg.gain));word(0xffeea6,uint16_t(cfg.integral));wide(0xffeeaa,cfg.sum);
   word(0xffffe6,uint16_t(cfg.counter));
   // A nested TPU0 takes its complete clock/decay/latch chain too.
   byte(0xffec14,1);byte(0xffee70,128);wide(0xffec10,7);byte(0xffec23,1);
   wide(0xffee7e,1);wide(0xffee82,0xffffffff);wide(0xffee86,0x34);
   word(0xffed6c,45);word(0xffed6e,17);byte(0xffff51,0);byte(0xffff8a,0xf0);
   for(unsigned n=0;n<4;++n){byte(0xffec16+n,uint8_t(1u<<n));byte(0xffec24+n,uint8_t(128u>>n));}
  }
  unsigned count=0,liveMask=0;bool injected=false;
  const auto setByte=[&](uint32_t at,uint8_t value){a->memory[at]=b->memory[at]=value;};
  const auto setWord=[&](uint32_t at,uint16_t value){setByte(at,uint8_t(value>>8));setByte(at+1,uint8_t(value));};
  const auto setLong=[&](uint32_t at,uint32_t value){for(unsigned n=0;n<4;++n)setByte(at+n,uint8_t(value>>(24-8*n)));};
  do {
   if(!injected&&count==injectAt){
    if(injectedVector<64)a->pending_irqs=b->pending_irqs=1ull<<injectedVector;
    else a->pending_irqs_high=b->pending_irqs_high=1ull<<(injectedVector-64);
    injected=true;
   }
   if(cfg.live){
    const auto once=[&](unsigned bit,uint32_t pc,auto&& change){
     if(!(liveMask&(1u<<bit))&&a->getPC()==pc){change();liveMask|=1u<<bit;++mutations;}
    };
    // These changes are external resource transitions selected by the oracle's
    // phase, not a synchronization of native CPU state.
    once(0,0x2850,[&]{setByte(0xffee9a,255);}); // Latched source already sampled: keep feedback path.
    once(1,0x2862,[&]{setByte(0xffee97,0x80);}); // RMW must preserve the live flag byte.
    once(2,0x28bc,[&]{setLong(0xffee78,0x87654321);}); // Timer is already retained; later reads remain live.
    once(3,0x28ce,[&]{setWord(0xffee9c,0xfe00);}); // Samples are sampled here; wrap/divzero downstream.
    once(4,0x25fe,[&]{setWord(0xffffe6,40000);}); // Counter remains live inside its child.
    once(5,0x28e4,[&]{setWord(0xffee9e,19000);}); // Counter retained; baseline remains live.
    once(6,0x291c,[&]{setLong(0xffeea0,0xdeadbeef);});
    once(7,0x2952,[&]{setWord(0xffeea6,0x7fff);setWord(0xffeea4,0x4000);});
   }
   if(count==3){runtime.resetMetrics();++metricResets;}
   x.events.clear();y.events.clear();track=true;runtime.step(*b);track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"vector="<<vector<<" boundary="<<count<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("SCI receive phase clocks differ");}
   auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);
   check(std::equal(a->memory+0xffdc00,a->memory+0xfff700,b->memory+0xffdc00),"IRQ evolving resource memory differs");
   if(expected!=actual){std::cerr<<"vector="<<vector<<" boundary="<<count<<" producer="<<producer<<" consumer="<<consumer<<" old/new transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto& rows:{expected,actual}){for(const auto& e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ' ;std::cerr<<'\n';}std::cerr<<" inject="<<injectAt<<" old/new pc="<<std::hex<<a->getPC()<<'/'<<b->getPC()<<std::dec<<'\n';throw std::runtime_error("IRQ observable transfer/value/order/time differs");}
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"IRQ admission/priority differs");
   // The whole SCI receive graph, including every child, leaves these CPU fields and bytes untouched.
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
  if(cfg.live){check(liveMask==0xff,"Live SCI resource mutation graph coverage incomplete");++liveCases;}
  components|=runtime.nativeSerialReceiveComponents;
  ++cases;if(injected)++nested;return count;
 };
 auto run=[&](unsigned injectAt=~0u,unsigned vector=32,unsigned mask=0){return verify(89,cfg.producer,cfg.consumer,cfg.status,0xa5,injectAt,false,mask,1,vector);};
 for(unsigned data=0;data<256;++data)for(unsigned producer=0;producer<256;++producer)for(bool full:{false,true}){
  cfg=Config{};cfg.data=data;cfg.producer=producer;cfg.consumer=(producer+unsigned(full?1:2))&255;run();
 }
 for(unsigned producer=0;producer<256;++producer)for(unsigned consumer=0;consumer<256;++consumer){
  cfg=Config{};cfg.data=0x7f;cfg.producer=producer;cfg.consumer=consumer;run();
 }
 for(unsigned status=0;status<256;++status){cfg=Config{};cfg.status=status;cfg.data=status;run();}
 // Reset admission and the two-source latch value are independent inputs.
 for(unsigned active:{0u,1u,255u})for(unsigned source=0;source<256;++source){
  cfg=Config{};cfg.active=active;cfg.source=source;cfg.counter=0xffff;run();
 }
 for(unsigned samples:{0u,1u,0x7dffu,0x7e00u,0xfdffu,0xfe00u,0xffffu})
 for(unsigned gain:{0u,0x7fu,0x80u,0x7ffu,0x800u,0x3fffu,0x4000u,0x7fffu,0x8000u,0xffffu})
 for(unsigned integral:{0u,1u,0x17ffu,0x1800u,0x7fffu,0x8000u,0xffffu})
 for(unsigned sum:{0u,1u,0x7fffffffu,0x80000000u,0xffffffffu}){
  cfg=Config{};cfg.samples=samples;cfg.gain=gain;cfg.integral=integral;cfg.sum=sum;
  cfg.estimate=uint32_t(rng());cfg.fraction=unsigned(rng())&65535;run();
 }
 for(unsigned pulseCount:{0u,0x7fffu,0xffffu})for(unsigned pulses:{0u,0x7fu,0x80u,0xffu,0x100u,0x7fffu,0x8000u,0xffffu})for(unsigned fraction:{0u,0x7fffu,0x8000u,0xffffu}){
  cfg=Config{};cfg.pulseCount=pulseCount;cfg.pulses=pulses;cfg.fraction=fraction;cfg.samples=0xfe00;run();
 }
 for(unsigned elapsed:{0u,1u,15999u,16000u,16001u,32767u,32768u,65535u})
 for(unsigned counter:{0u,1u,0x7fffu,0x8000u,0xffffu}){
  cfg=Config{};cfg.counter=counter;cfg.baseline=uint16_t(counter-elapsed);run();
 }
 cfg=Config{};cfg.gain=0x80;cfg.integral=0x7fff;const auto full=run();
 for(unsigned at=1;at<full;++at)run(at);
 cfg=Config{};cfg.live=true;run();
 check(liveCases==1&&mutations==8,"Live resource scenario coverage incomplete");
 check(components==0xf,"SCI receive/F8 child graph coverage incomplete");
 check(!allocations&&!b->interpretedSteps,"Native SCI receive allocated or called the interpreter");
 std::cout<<"PASS: "<<cases<<" complete CPU-free SCI receive calls; "<<boundaries<<" independent H8 phases; "<<nested<<" nested TPU0 preemptions; components="<<std::hex<<components<<std::dec<<"; "<<metricResets<<" metric resets; "<<liveCases<<" live cases / "<<mutations<<" resource changes; all byte transfers/timestamps/resource memory and caller state exact; no allocations/interpreter/CPU stack writes\n";
 return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
