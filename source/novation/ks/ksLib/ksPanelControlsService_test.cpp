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
 ks::LiveMidiController state(*b);ks::RuntimeController runtime(*b,state);auto& foreground=runtime.panelControlsForeground;std::mt19937 rng(0x52455345);
 const std::array<unsigned,7> roots{{0x13ae,0x13b4,0x13be,0x1886,0x1624,0x1646,0x156e}};
 const std::array<unsigned,7> returns{{0x13b2,0x13bc,0x13f6,0x18f6,0x1644,0x165a,0x1576}};
 struct Config{unsigned timer=0x1234,half=5,activity=3,quarter=7,delta=128,polarity=0,port=0,history=255,group=36;}cfg;
 uint64_t cases=0,boundaries=0,nested=0,resets=0,mutations=0,guardCases=0;uint32_t components=0;
 auto verify=[&](unsigned kind,unsigned savedValue,unsigned flags,unsigned injectAt=~0u,unsigned vector=32,bool live=false){
  check(!foreground.active()&&!runtime.activeNativeInterrupts(),"Previous reset did not retire");runtime.resetMetrics();foreground.resetMetrics();
  std::array<int32_t,8> saved{};for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());
  const unsigned valueRegister=0;a->regs[valueRegister].r=b->regs[valueRegister].r=int16_t(savedValue);saved[valueRegister]=b->regs[valueRegister].er;
  a->setSP(0xfff800);b->setSP(0xfff800);saved[7]=int32_t(0xfff800);
  for(auto*h:{a.get()}){h->pc=h->makepc(roots[kind]);h->ccr=uint8_t(flags);h->exr=0xa0;h->cycles=1000;h->indent=1;h->lastread=0x12345;h->lastwrite=0x23456;h->interruptMode2=true;h->pending_irqs=h->pending_irqs_high=0;for(auto&p:h->interruptPriority)p=0;h->interruptPriority[vector]=6;}
  b->pc=roots[kind];b->ccr=uint8_t(flags);b->exr=0xa0;b->cycles=1000;b->indent=1;b->lastread=0x12345;b->lastwrite=0x23456;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[vector]=6;
  const auto byte=[&](uint32_t address,uint8_t value){a->memory[address]=b->memory[address]=value;};
  const auto word=[&](uint32_t address,uint16_t value){byte(address,uint8_t(value>>8));byte(address+1,uint8_t(value));};
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));
   std::fill(m+0x1537c,m+0x15800,uint8_t(0x7b));
   for(unsigned n=0;n<36;++n){m[0xb918 + n]=uint8_t(n);m[0xb93c + n]=uint8_t(n&1);}
   m[0xffec14]=0;m[0xffee70]=128;m[0xffec10]=m[0xffec11]=m[0xffec12]=0;m[0xffec13]=7;
   for(unsigned n=0;n<4;++n){m[0xffee7a + n]=0xff;m[0xffee7e + n]=uint8_t(n==3);m[0xffee82 + n]=0;m[0xffee86 + n]=uint8_t(n==3);}
   m[0xffec23]=1;m[0xffee97]=0x1c;m[0xffee96]=1;m[0xffee9a]=0;m[0xffff51]=0;m[0xffff55]=0xff;m[0xffff54]=12;m[0xffff59]=0xe0;m[0xffff50]=31;
   m[0xffee76]=m[0xffee77]=m[0xffee78]=m[0xffee79]=255;m[0xffed6c]=m[0xffed6e]=0;m[0xffed6d]=45;m[0xffed6f]=17;
   m[0xffec64]=m[0xffec65]=m[0xffec66]=m[0xffec67]=0;m[0xffff8c]=0xb7;m[0xffff8a]=0xf0;m[0xffff8d]=0xf8;
   m[0xffffe6]=0x12;m[0xffffe7]=0x34;
   for(unsigned n=0;n<8;++n){m[0xb960 + n*2]=0xaa;m[0xb961 + n*2]=0xaa;}
   for(unsigned n=0;n<64;++n){auto at=0xb970 + 4*n;m[at]=3;m[at+1]=uint8_t(n+36);m[at+2]=0xff;m[at+3]=0xe2;}
   for(unsigned n=0;n<4;++n){m[0xffec16 + n]=uint8_t(1u<<n);m[0xffec24 + n]=uint8_t(128u>>n);}
  }
  for(unsigned n=0;n<4;++n){word(0xffff90 + 2*n,0x8000);word(0x15652 + 2*n,0x8000);word(0x1560a + 2*n,0);byte(0x1569a + 2*n,0);}
  byte(0x156e2,uint8_t(cfg.group));byte(0x156e3,0);byte(0x156e4,255);byte(0xffff98,0x73);
  word(0xfffff6,uint16_t(cfg.timer));byte(0xffee73,uint8_t(cfg.half));byte(0xffee74,uint8_t(cfg.activity));byte(0xffee72,uint8_t(cfg.quarter));byte(0xffee75,uint8_t(cfg.delta));
  byte(0x169fa,uint8_t(cfg.polarity));byte(0xffff50,uint8_t(cfg.port));byte(0xffff53,uint8_t(cfg.port));byte(0x1537a,uint8_t(cfg.history));
  for(unsigned index:{0x1537cu,0x1537eu,0x15406u,0x15408u,0x156e8u,0x156eau})word(index,uint16_t(rng()));
  byte(0xfff800,0);byte(0xfff801,0);byte(0xfff802,0x43);byte(0xfff803,0x38);
  const uint32_t target=live?0x433a:0x4338;unsigned count=0,liveMask=0;bool injected=false;
  do{
   if(count==injectAt){if(vector<64)a->pending_irqs=b->pending_irqs=1ull<<vector;else a->pending_irqs_high=b->pending_irqs_high=1ull<<(vector-64);injected=true;}
   if(live){
    const auto once=[&](unsigned bit,uint32_t pc,auto&& change){if(!(liveMask&(1u<<bit))&&a->getPC()==pc){change();liveMask|=1u<<bit;++mutations;}};
    if(kind==0)once(0,0x13b0,[&]{byte(0xffff53,0xff);});
    if(kind==2){
     once(0,0x13d0,[&]{byte(0x169fa,0xff);byte(0xffff50,0x83);});
     once(1,0x13dc,[&]{byte(0x1537a,0x44);});
    }
    if(kind==3){
     once(0,0x188a,[&]{word(0xfffff6,0x8123);});
     once(1,0x189e,[&]{byte(0xffee74,255);});
     once(2,0x18c4,[&]{byte(0xffee72,0x7f);});
     once(3,0x18d4,[&]{byte(0xffee75,0xff);});
    }
    if(kind==4)once(0,0x1646,[&]{byte(0x156e2,0x7f);});
    if(kind==4||kind==5)once(1,0x164c,[&]{byte(0x156e2,3);});
    once(7,returns[kind],[&]{byte(0xfff803,0x3a);});
   }
   if(count==3){runtime.resetMetrics();foreground.resetMetrics();++resets;}
   x.events.clear();y.events.clear();track=true;runtime.step(*b);track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"kind="<<kind<<" boundary="<<count<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("Panel controls phase clocks differ");}
   auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);
   if(expected!=actual){std::cerr<<"kind="<<kind<<" boundary="<<count<<" saved="<<savedValue<<" flags="<<flags<<" old/new transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto& rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}std::cerr<<"old/new PC="<<std::hex<<a->getPC()<<'/'<<b->getPC()<<std::dec<<'\n';throw std::runtime_error("Panel controls byte transfer/value/order/time differs");}
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"Panel controls IRQ admission/priority differs");
   // The first logical call (State) or retained scope (standalone reset)
   // replaces a nonnegative incoming write history with internal-RAM history.
   if(!count&&!injected&&(kind==2||kind==4))check(a->lastwrite==b->lastwrite&&b->lastwrite==-1,"Controls first retained-scope history differs");
   check(std::equal(a->memory+0xffdc00,a->memory+0xfff700,b->memory+0xffdc00),"Panel controls evolving resource memory differs");
   if(b->pc!=target||b->indent!=0){
    check(b->pc==roots[kind]&&b->getSP()==0xfff800&&b->ccr==uint8_t(flags),"Native controls used a CPU continuation");
    for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Native controls used CPU registers");
   }
   for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Native controls wrote CPU stack");
   check(++count<8192,"Panel controls did not finish");++boundaries;
  }while(a->getPC()!=target||a->indent!=0);
  check(!foreground.active()&&!runtime.activeNativeInterrupts(),"Native controls left active continuation");
  check(foreground.started+foreground.initialActive==foreground.completed,"Reset metric reset lost continuation");
  check(runtime.nativeInterruptStarts+runtime.nativeInterruptInitial==runtime.nativeInterruptReturns,"Reset nested IRQ metric balance differs");
  for(unsigned n=0;n<8;++n)check(a->regs[n].er==b->regs[n].er,"Returned controls register differs");
  check(a->ccr==b->ccr&&a->exr==b->exr&&a->indent==b->indent&&a->lastread==b->lastread&&a->lastwrite==b->lastwrite,"Returned controls flags/bus history differ");
  check(b->getSP()==0xfff804,"Reset caller frame return differs");
  if(injected||live)check(std::equal(a->memory,a->memory+0xfff700,b->memory)&&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),"Reset full resource memory differs");
  if(live)check(liveMask&128,"Controls live outer caller frame was not observed");
  components|=foreground.components;++cases;if(injected)++nested;return count;
 };
 for(unsigned port=0;port<256;++port){cfg=Config{};cfg.port=port;verify(0,port,port);}
 for(unsigned polarity=0;polarity<256;++polarity)for(unsigned port=0;port<256;++port)for(unsigned history:{0u,64u,128u,255u}){cfg=Config{};cfg.polarity=polarity;cfg.port=port;cfg.history=history;verify(2,polarity^port,port);}
 for(unsigned timer=0;timer<1024;++timer)for(unsigned half:{0u,1u,127u,128u,255u})for(unsigned activity:{0u,1u,3u,4u,13u,14u,255u})for(unsigned movement:{0u,1u,127u,128u,255u})for(unsigned previousDelta:{0u,128u}){cfg=Config{};cfg.timer=timer;cfg.half=half;cfg.activity=activity;cfg.quarter=uint8_t((timer>>2)+movement);cfg.delta=previousDelta;verify(3,timer,timer&255);}
 for(unsigned timer=0;timer<65536;++timer){cfg=Config{};cfg.timer=timer;verify(3,timer,timer&255);}
 for(unsigned group=0;group<256;++group)for(unsigned flags=0;flags<256;++flags){cfg=Config{};cfg.group=group;verify(4,(group<<8)|flags,flags);verify(5,(flags<<8)|group,flags);}
 for(unsigned value=0;value<65536;++value){cfg=Config{};cfg.group=value&255;verify(4,value,value&255);}
 for(unsigned kind=0;kind<7;++kind)for(unsigned flags=0;flags<256;++flags)for(unsigned value:{0u,1u,0x7fffu,0x8000u,0xffffu}){cfg=Config{};cfg.timer=flags*257;cfg.polarity=flags;cfg.port=flags;cfg.history=value&255;verify(kind,value,flags);}
 for(unsigned kind=0;kind<7;++kind){cfg=Config{};const auto phases=verify(kind,0x8a5c,0xe7);for(unsigned vector:{28u,32u,88u,89u,90u,91u})for(unsigned at=0;at<phases;++at)verify(kind,0x8a5c,0xe7,at,vector);}
 for(unsigned kind=0;kind<7;++kind){cfg=Config{};verify(kind,0,0xff,~0u,32,true);}
 // Diagnostic frames must retain their explicit compatibility path. Rejected
 // admissions may not claim a job, clock a phase or touch caller/resources.
 for(unsigned kind=0;kind<7;++kind)for(unsigned variant=0;variant<10;++variant){
  check(!foreground.active(),"Guard case found an active reset");foreground.resetMetrics();
  b->pc=roots[kind];b->setSP(0xfff800);b->indent=1;b->cycles=1000;b->ccr=0xf5;b->exr=0xa0;b->lastread=0x12345;b->lastwrite=0x23456;
  b->pending_irqs=0x123;b->pending_irqs_high=0x456;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->interruptMode2=true;
  switch(variant){
  case 0:b->nativeFetchClocksOnly=false;break;case 1:b->nativeLowBase=nullptr;break;
  case 2:b->interruptMode2=false;break;case 3:b->indent=0;break;case 4:b->indent=-1;break;
  case 5:b->setSP(0xfff801);break;case 6:b->setSP(0xfff3fe);break;
  case 7:b->setSP(0xfffbfe);break;case 8:b->setSP(0xa18000);break;
  case 9:b->pc=0x4488;break;
  }
  std::array<int32_t,8> saved{};for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;
  const auto pc=b->pc;const auto depth=b->indent;const auto cycles=b->cycles;
  x.events.clear();y.events.clear();track=true;const auto handled=foreground.step(*b);track=false;
  check(!handled&&!foreground.active()&&!foreground.started&&!foreground.completed&&!foreground.phases&&!foreground.components,"Rejected reset admission claimed a continuation");
  check(b->pc==pc&&b->indent==depth&&b->cycles==cycles&&b->ccr==0xf5&&b->exr==0xa0&&b->lastread==0x12345&&b->lastwrite==0x23456,"Rejected reset admission changed caller state");
  check(b->pending_irqs==0x123&&b->pending_irqs_high==0x456&&y.events.empty(),"Rejected reset admission changed resources/pending IRQs");
  for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Rejected reset admission changed CPU registers");
  ++guardCases;
 }
 check(guardCases==70,"Reset admission guard coverage incomplete");
 check(components==127&&!allocations&&!b->interpretedSteps,"Native controls graph incomplete, allocated or interpreted");
 std::cout<<"PASS: "<<cases<<" CPU-free panel control calls; "<<boundaries<<" independent H8 phases; "<<nested<<" native IRQ preemptions; "<<resets<<" metric resets; "<<mutations<<" live resource changes; "<<guardCases<<" diagnostic admission guards; all seven roots, encoder/pedal/ADC matrices and all flags, exact bytes/clocks/resources and final caller ABI; zero allocations/interpreter/internal CPU stack writes\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
