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
 uint64_t cases=0,boundaries=0,nested=0,mutations=0,metricResets=0,lcdBusyCases=0,lcdBusyReads=0,liveCases=0;
 struct Config{unsigned divider=0,counter=0,pattern=0,flags=0,delay=0,holdoff=0,producer=45,consumer=17,lcdBusyReads=0,lcdReply=0;bool scan=false,carry=false,output=false,injectWait=false,injectLcd=false,live=false;unsigned injectPriority=7;}cfg;
 uint32_t components=0;
 auto verify=[&](unsigned vector,unsigned producer,unsigned consumer,unsigned status,unsigned flags,unsigned injectAt=~0u,bool mutate=false,unsigned mask=0,unsigned priority=1,unsigned injectedVector=32,unsigned timerVariant=0){
  check(!runtime.activeNativeInterrupts(),"Previous native interrupt did not retire");runtime.resetMetrics();
  std::array<int32_t,8> saved{};for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());
  a->setSP(0xfff800);b->setSP(0xfff800);saved[7]=int32_t(0xfff800);
  for(auto*h:{a.get()}){h->pc=h->makepc(0x4338);h->ccr=uint8_t(flags);h->exr=uint8_t(mask);h->cycles=1000;h->indent=0;h->lastread=h->lastwrite=-1;h->interruptMode2=true;h->pending_irqs=h->pending_irqs_high=0;for(auto&p:h->interruptPriority)p=0;h->interruptPriority[vector]=uint8_t(priority);h->interruptPriority[injectedVector]=uint8_t(cfg.injectPriority);}
  b->pc=0x4338;b->ccr=uint8_t(flags);b->exr=uint8_t(mask);b->cycles=1000;b->indent=0;b->lastread=b->lastwrite=-1;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[vector]=uint8_t(priority);b->interruptPriority[injectedVector]=uint8_t(cfg.injectPriority);
  if(vector<64)a->pending_irqs=b->pending_irqs=1ull<<vector;else a->pending_irqs_high=b->pending_irqs_high=1ull<<(vector-64);
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));m[0xffff8c]=uint8_t(status);m[0xffff8a]=0xf0;m[0xffed68]=0x7b;
   m[0xffed6c]=uint8_t(producer>>8);m[0xffed6d]=uint8_t(producer);m[0xffed6e]=uint8_t(consumer>>8);m[0xffed6f]=uint8_t(consumer);
   for(unsigned n=0;n<256;++n)m[0xffed70+n]=uint8_t(n*149+flags);
   m[0xffec14]=cfg.scan?0:1;m[0xffee70]=uint8_t(cfg.divider);
   for(unsigned n=0;n<4;++n){m[0xffec10+n]=uint8_t(cfg.counter>>(24-8*n));m[0xffee7a+n]=cfg.carry?0xff:0;m[0xffee7e + n]=uint8_t(n==3);m[0xffee82+n]=0xff;m[0xffee86+n]=uint8_t(n==3?0x34:0);}
   m[0xffec23]=uint8_t(cfg.holdoff);m[0xffee97]=cfg.output?2:0;m[0xffee9a]=0xff;
   m[0xffee76]=m[0xffee77]=m[0xffee78]=m[0xffee79]=0xff;
   m[0xffff51]=cfg.lcdBusyReads?0x80:uint8_t(cfg.lcdReply);const auto current=uint16_t(0x5555u*(cfg.pattern>>2)),previous=uint16_t(0x5555u*(cfg.pattern&3));
   m[0xffff55]=uint8_t(current>>8);m[0xffff54]=uint8_t((current>>8&3)<<2);m[0xffff59]=uint8_t(current&0xe0);m[0xffff50]=uint8_t(current&31);
   for(unsigned n=0;n<8;++n){m[0xb960+2*n]=uint8_t(previous>>8);m[0xb961+2*n]=uint8_t(previous);}
   for(unsigned n=0;n<64;++n){const auto at=0xb970+4*n;const auto start=uint16_t(0-cfg.delay);m[at]=uint8_t(cfg.flags);m[at+1]=uint8_t(n+36);m[at+2]=uint8_t(start>>8);m[at+3]=uint8_t(start);}
   m[0x156e8]=1;m[0x156e9]=0xfe;m[0x156ea]=m[0x156eb]=0;
   for(unsigned n=0;n<4;++n){m[0xffec16+n]=uint8_t(1u<<n);m[0xffec24+n]=uint8_t(128u>>n);}

  }
  unsigned count=0,lcdReads=0,liveMask=0;bool injected=false,lcdReleased=false;
  const auto setByte=[&](uint32_t at,uint8_t value){a->memory[at]=b->memory[at]=value;};
  const auto setWord=[&](uint32_t at,uint16_t value){setByte(at,uint8_t(value>>8));setByte(at+1,uint8_t(value));};
  const auto setLong=[&](uint32_t at,uint32_t value){for(unsigned n=0;n<4;++n)setByte(at+n,uint8_t(value>>(24-8*n)));};
  do {
   // Release the external LCD input only after both engines observed the same
   // configured number of busy reads, including repeated status-byte calls.
   if(cfg.lcdBusyReads&&!lcdReleased&&lcdReads==cfg.lcdBusyReads){setByte(0xffff51,uint8_t(cfg.lcdReply));lcdReleased=true;}
   if(!injected&&(count==injectAt||(cfg.injectWait&&a->getPC()==0x1124)||(cfg.injectLcd&&lcdReads==1))){
    if(injectedVector<64)a->pending_irqs=b->pending_irqs=1ull<<injectedVector;
    else a->pending_irqs_high=b->pending_irqs_high=1ull<<(injectedVector-64);
    injected=true;
   }
   if(mutate&&count==5){for(auto*m:{a->memory,b->memory}){m[0xffff8c]^=0xff;m[0xffed6d]=uint8_t(producer+17);m[0xffed6f]=uint8_t(consumer+37);}++mutations;}
   if(cfg.live){
    const auto once=[&](unsigned bit,uint32_t pc,auto&& change){
     if(!(liveMask&(1u<<bit))&&a->getPC()==pc){change();liveMask|=1u<<bit;++mutations;}
    };
    // The reference PC selects a one-time external resource change. The native
    // graph still receives only byte resources, with no CPU-state projection.
    once(0,0x1ba4,[&]{ // Upper pins were sampled; lower pins and later pages are live.
     setByte(0xffff55,0xaa);setByte(0xffff54,8);setByte(0xffff59,0xa0);setByte(0xffff50,10);
    });
    once(1,0x1e9e,[&]{ // Retain the sampled tick; observe new start and velocity flags.
     const auto record=uint32_t(a->regs[3].er)&0xffffff;
     setWord(0xffee76,25);setWord(record+2,0xfff0);setByte(record,0);
    });
    once(2,0x1f5c,[&]{ // Queue index is sampled now; the event key is already latched.
     setWord(0x156e8,0x1ff);setByte((uint32_t(a->regs[3].er)&0xffffff)+1,100);
    });
    once(3,0x27dc,[&]{ // Phase, increment and pulse count were already sampled.
     setLong(0xffee7a,0x13579bdf);setLong(0xffee7e,0x2468ace0);setWord(0xffee78,0x2222);
    });
    once(4,0x27ec,[&]{setByte(0xffee97,4);}); // Keep the admitted F8; re-read run flags later.
    once(5,0x1124,[&]{ // Retain MIDI's producer while observing its live consumer.
     setWord(0xffed6c,201);setWord(0xffed6e,8);
    });
    once(6,0x2824,[&]{setByte(0xffee97,0xff);setByte(0xffee9a,0x7f);});
    once(7,0x730,[&]{ // Preserve a completed cell; merge live pending and later bank.
     setByte(0xffec16,0x66);setByte(0xffec1b,0x80);setByte(0xffec24,1);
    });
    once(8,0x774,[&]{setByte(0xffec23,9);}); // Holdoff was sampled as one: expiry still latches.
   }
   if(count==3){runtime.resetMetrics();++metricResets;}
   x.events.clear();y.events.clear();track=true;runtime.step(*b);track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"vector="<<vector<<" boundary="<<count<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("TPU0 phase clocks differ");}
   auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);
   check(std::equal(a->memory+0xffdc00,a->memory+0xfff700,b->memory+0xffdc00),"IRQ evolving resource memory differs");
   if(expected!=actual){std::cerr<<"vector="<<vector<<" boundary="<<count<<" producer="<<producer<<" consumer="<<consumer<<" old/new transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto& rows:{expected,actual}){for(const auto& e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ' ;std::cerr<<'\n';}std::cerr<<" inject="<<injectAt<<" old/new pc="<<std::hex<<a->getPC()<<'/'<<b->getPC()<<std::dec<<'\n';throw std::runtime_error("IRQ observable transfer/value/order/time differs");}
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"IRQ admission/priority differs");
   for(const auto& event:expected)if(!std::get<0>(event)&&std::get<1>(event)==0xffff51)++lcdReads;
   // The whole TPU0 graph, including every child, leaves these CPU fields and bytes untouched.
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
  if(cfg.lcdBusyReads){
   check(lcdReleased&&lcdReads==cfg.lcdBusyReads+2,"LCD busy-to-ready polling was not exercised exactly");
   ++lcdBusyCases;lcdBusyReads+=cfg.lcdBusyReads;
  }
  if(cfg.live){check(liveMask==0x1ff,"Live resource mutation graph coverage incomplete");++liveCases;}
  components|=runtime.nativeTpuComponents;
  ++cases;if(injected)++nested;return count;
 };
 auto run=[&](unsigned injectAt=~0u,unsigned vector=91,unsigned mask=0){return verify(32,cfg.producer,cfg.consumer,0xb7,0xa5,injectAt,false,mask,6,vector);};
 for(unsigned divider=0;divider<256;++divider)for(unsigned residue=0;residue<8;++residue)for(bool scan:{false,true}){
  cfg=Config{};cfg.divider=divider;cfg.counter=residue;cfg.scan=scan;run();
 }
 for(unsigned pattern=0;pattern<16;++pattern)for(unsigned delay:{0u,1u,2u,3u,9u,10u,11u,30u,120u,65535u})for(unsigned flags=0;flags<4;++flags){
  cfg=Config{};cfg.divider=128;cfg.counter=7;cfg.holdoff=1;cfg.carry=cfg.output=cfg.scan=true;cfg.pattern=pattern;cfg.flags=flags;cfg.delay=delay;run();
 }
 cfg=Config{};cfg.divider=128;cfg.counter=7;cfg.holdoff=1;cfg.carry=cfg.output=cfg.scan=true;cfg.pattern=14;cfg.flags=3;cfg.delay=30;
 const auto full=run();
 for(unsigned at=1;at<full;++at)run(at);
 for(unsigned mask=0;mask<6;++mask)run(~0u,91,mask|0xa0);
 cfg=Config{};cfg.divider=128;cfg.carry=cfg.output=true;cfg.producer=255;cfg.consumer=0;cfg.injectWait=true;run(~0u,90);
 for(unsigned busy:{1u,2u,5u}){
  cfg=Config{};cfg.divider=128;cfg.counter=7;cfg.holdoff=1;cfg.carry=cfg.output=cfg.scan=true;cfg.pattern=14;cfg.flags=3;cfg.delay=30;
  cfg.lcdBusyReads=busy;cfg.lcdReply=0x14;cfg.injectLcd=true;run();
 }
 for(unsigned busy:{0u,3u}){
  cfg=Config{};cfg.divider=128;cfg.counter=7;cfg.holdoff=1;cfg.carry=cfg.output=cfg.scan=true;cfg.pattern=14;cfg.flags=3;cfg.delay=30;
  cfg.live=true;cfg.lcdBusyReads=busy;cfg.lcdReply=0x14;cfg.injectLcd=busy!=0;run();
 }
 check(lcdBusyCases==4&&lcdBusyReads==11&&liveCases==2&&mutations==18,"External resource scenario coverage incomplete");
 check(components==0x7ff,"TPU0 child graph coverage incomplete");
 check(!allocations&&!b->interpretedSteps,"Native IRQ allocated or called the interpreter");
 std::cout<<"PASS: "<<cases<<" complete CPU-free TPU0 calls; "<<boundaries<<" independent H8 phases; "<<nested<<" preemptions; components="<<std::hex<<components<<std::dec<<"; "<<metricResets<<" metric resets; "<<lcdBusyCases<<" busy-to-ready LCD cases / "<<lcdBusyReads<<" busy reads; "<<liveCases<<" live cases / "<<mutations<<" resource changes; all byte transfers/timestamps/resource memory and caller state exact; no allocations/interpreter/CPU stack writes\n";

 return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
