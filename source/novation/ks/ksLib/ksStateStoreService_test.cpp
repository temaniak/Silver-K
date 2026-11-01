#include "ksStateStoreForeground.h"
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
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}check(!ks::controllerReferenceAvailable(),"Synchronized reference factory linked");ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();
 for(unsigned base:{0u,0xa00000u}){a->loadmem(rom.data().data(),unsigned(rom.data().size()),base);b->loadmem(rom.data().data(),unsigned(rom.data().size()),base);}decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);g_dasm=false;
 Bus<H8SDevice>x;Bus<ks::ControllerBus>y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);
 b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);ks::StateStoreForeground foreground;std::mt19937 rng(0x53544f52);
 using K=ks::StateStoreService::Kind;constexpr std::array<unsigned,17>roots{{0x71ba,0x687e,0x688e,0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6,0x6e42,0x6e52,0x6e5e,0x6e7a,0x6e8a,0x6e96,0x6eb0,0x6eba}};
 uint64_t cases=0,phases=0,interrupts=0,metricResets=0,liveCases=0,guards=0;uint32_t components=0,kindMask=0;
 struct Config{K kind=K::MultiSerialize;unsigned seed=0,flags=0xa5;uint32_t source=0x30000;bool live=false,livePointer=false,liveCaller=false;}cfg;
 const auto run=[&](unsigned injectAt=~0u,unsigned vector=32){
  check(!foreground.active()&&!runtime.activeNativeInterrupts(),"Previous store graph active");runtime.resetMetrics();foreground.resetMetrics();
  const auto root=roots[unsigned(cfg.kind)];std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)a->regs[n].er=b->regs[n].er=int32_t(rng());
  a->regs[0].er=b->regs[0].er=int32_t(cfg.source);a->regs[4].er=b->regs[4].er=int32_t(cfg.source);a->regs[2].rl=b->regs[2].rl=int8_t(1+cfg.seed%6);
  a->setSP(0xfff800);b->setSP(0xfff800);for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;
  a->pc=a->makepc(root);b->pc=root;a->ccr=b->ccr=uint8_t(cfg.flags);a->exr=b->exr=0xa0;a->cycles=b->cycles=1000;a->indent=b->indent=1;a->lastread=b->lastread=0x12345;a->lastwrite=b->lastwrite=0x23456;a->interruptMode2=b->interruptMode2=true;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;
  for(auto&p:a->interruptPriority)p=0;for(auto&p:b->interruptPriority)p=0;a->interruptPriority[vector]=b->interruptPriority[vector]=6;
  const auto byte=[&](uint32_t at,uint8_t v){a->memory[at&0xffffff]=b->memory[at&0xffffff]=v;};
  const auto word=[&](uint32_t at,uint16_t v){byte(at,uint8_t(v>>8));byte(at+1,uint8_t(v));};
  const auto wide=[&](uint32_t at,uint32_t v){word(at,uint16_t(v>>16));word(at+2,uint16_t(v));};
  for(unsigned i=0;i<1024;++i)byte(0x169fc + i,uint8_t(cfg.seed*17+i*31));
  for(unsigned i=0;i<256;++i){byte(cfg.source+i,uint8_t(cfg.seed*13+i*37+(cfg.seed>>8)));byte(0x168fc + i,uint8_t(cfg.seed*43+i*59));}
  for(unsigned p=0;p<5;++p)for(unsigned n=0;n<20;++n)byte(0x174ae + 20*p+n,uint8_t(cfg.seed*61+p*79+n*97));
  for(unsigned p=0;p<4;++p)word(0x167b0 + 2*p,uint16_t(cfg.seed*197+p*283));
  for(unsigned n=0;n<17;++n)byte(0x1733c + n,uint8_t(cfg.seed*101+n*107));
  byte(0x167d2,uint8_t(cfg.seed));byte(0x167d3,uint8_t(cfg.seed*5));wide(0x167bc,cfg.source);wide(0x1748e,cfg.source);
  // Complete closed interrupt fixtures, independent of caller scratch values.
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));
   for(unsigned n=0;n<36;++n){m[0xb918 + n]=uint8_t(n);m[0xb93c + n]=uint8_t(n&1);}
   m[0xffec14]=0;m[0xffee70]=128;m[0xffec10]=m[0xffec11]=m[0xffec12]=0;m[0xffec13]=7;
   for(unsigned n=0;n<4;++n){m[0xffee7a + n]=0xff;m[0xffee7e + n]=uint8_t(n==3);m[0xffee82 + n]=0;m[0xffee86 + n]=uint8_t(n==3);}
   m[0xffec23]=1;m[0xffee97]=0x1c;m[0xffee96]=1;m[0xffee9a]=0;m[0xffff51]=0;m[0xffff55]=0xff;m[0xffff54]=12;m[0xffff59]=0xe0;m[0xffff50]=31;
   m[0xffee76]=m[0xffee77]=m[0xffee78]=m[0xffee79]=255;m[0xffed6c]=m[0xffed6e]=0;m[0xffed6d]=45;m[0xffed6f]=17;
   m[0xffec64]=m[0xffec65]=m[0xffec66]=m[0xffec67]=0;m[0xffff8c]=0xb7;m[0xffff8a]=0xf0;m[0xffff8d]=0xf8;m[0xffffe6]=0x12;m[0xffffe7]=0x34;
   for(unsigned n=0;n<8;++n){m[0xb960 + n*2]=0xaa;m[0xb961 + n*2]=0xaa;}
   for(unsigned n=0;n<64;++n){auto at=0xb970 + 4*n;m[at]=3;m[at+1]=uint8_t(n+36);m[at+2]=0xff;m[at+3]=0xe2;}
   for(unsigned n=0;n<4;++n){m[0xffec16 + n]=uint8_t(1u<<n);m[0xffec24 + n]=uint8_t(128u>>n);}
  }
  for(auto at:{0x1537cu,0x15406u,0x156e8u}){word(at,2);word(at+2,0);}
  for(unsigned n=0;n<4;++n){word(0xffff90 + 2*n,0x8000);word(0x15652 + 2*n,0x8000);word(0x1560a + 2*n,0);byte(0x1569a + 2*n,0);}
  byte(0x156e2,0);byte(0x156e3,0);byte(0x156e4,255);byte(0xffff98,0x73);wide(0xfff800,0x4338);
  for(unsigned n=0;n<256;++n)byte(0x34001+n,uint8_t(cfg.seed*71+n*83));
  unsigned count=0;bool injected=false;uint32_t returnTarget=0x4338;
  do{
   const auto pc=uint32_t(a->getPC());if(count==injectAt){if(vector<64)a->pending_irqs=b->pending_irqs=1ull<<vector;else a->pending_irqs_high=b->pending_irqs_high=1ull<<(vector-64);injected=true;}
   if(cfg.live){
    // Every scheduled source sample sees a fresh resource value. Output writes
    // are still made only by the original and native algorithms themselves.
    for(unsigned n=0;n<256;++n)byte(cfg.source+n,uint8_t(cfg.seed+count*13+n*17));
    for(unsigned p=0;p<5;++p)for(unsigned n=0;n<20;++n)byte(0x174ae + 20*p+n,uint8_t(cfg.seed+count*19+p*31+n*43));
    for(unsigned n=0;n<17;++n)byte(0x1733c + n,uint8_t(cfg.seed+count*47+n*53));
   }
   if(cfg.livePointer&&pc==0x68a2)wide(0x167bc,0x34001);
   if(cfg.liveCaller&&count==5){byte(0xfff803,0x3a);returnTarget=0x433a;}
   if(count==3){runtime.resetMetrics();foreground.resetMetrics();++metricResets;}
   x.events.clear();y.events.clear();track=true;if(runtime.activeNativeInterrupts()||b->pending_irqs||b->pending_irqs_high)runtime.step(*b);else check(foreground.step(*b),"Store graph declined owned phase");track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"kind="<<unsigned(cfg.kind)<<" PC="<<std::hex<<pc<<std::dec<<" phase="<<count<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("Store phase clocks differ");}
   const auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);
   if(expected!=actual){std::cerr<<"kind="<<unsigned(cfg.kind)<<" PC="<<std::hex<<pc<<std::dec<<" phase="<<count<<" transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Store byte/value/order/time differs");}
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"Store pending IRQ/priority differs");
   check(std::equal(a->memory+0x16790,a->memory+0x17530,b->memory+0x16790),"Store evolving state buffer differs");
   if(foreground.active()||runtime.activeNativeInterrupts()){
    check(b->pc==root&&b->getSP()==0xfff800&&b->ccr==uint8_t(cfg.flags),"Store graph used CPU continuation");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Store graph used CPU registers");
   }
   for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Store graph wrote CPU scratch stack");check(++count<8192,"Store graph stalled");++phases;
  }while(a->getPC()!=returnTarget||a->indent);
  check(!foreground.active()&&!runtime.activeNativeInterrupts()&&foreground.initial+foreground.started==foreground.completed,"Store continuation lifetime differs");
  for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"kind="<<unsigned(cfg.kind)<<" seed="<<cfg.seed<<" reg="<<n<<" old/new="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("Store final caller register differs");}
  if(a->ccr!=b->ccr){std::cerr<<"kind="<<unsigned(cfg.kind)<<" seed="<<cfg.seed<<" old/new flags="<<std::hex<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<" entry="<<cfg.flags<<std::dec<<'\n';throw std::runtime_error("Store final caller flags differ");}
  check(a->lastread==b->lastread&&a->lastwrite==b->lastwrite&&a->exr==b->exr&&a->getSP()==b->getSP()&&b->getSP()==0xfff804,"Store final caller history differs");
  if(injected||cfg.live||cfg.livePointer||cfg.liveCaller)check(std::equal(a->memory,a->memory+0xfff700,b->memory)&&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),"Store full resource state differs");
  components|=foreground.components;kindMask|=1u<<unsigned(cfg.kind);++cases;if(injected)++interrupts;if(cfg.live||cfg.livePointer||cfg.liveCaller)++liveCases;return count;
 };
 for(unsigned kind=0;kind<17;++kind)for(unsigned seed=0;seed<256;++seed){cfg=Config{};cfg.kind=K(kind);cfg.seed=seed;cfg.flags=seed;run();}
 for(unsigned kind=3;kind<9;++kind)for(unsigned seed=256;seed<65536;++seed){cfg=Config{};cfg.kind=K(kind);cfg.seed=seed;cfg.flags=seed&255;run();}
 for(unsigned kind=0;kind<17;++kind)for(uint32_t source:{0x30001u,0xffa30000u,0xa30001u,0xffee00u}){cfg=Config{};cfg.kind=K(kind);cfg.source=source;cfg.seed=127;run();}
 for(unsigned kind=0;kind<17;++kind){cfg=Config{};cfg.kind=K(kind);cfg.live=true;cfg.seed=255;run();cfg.live=false;const auto count=run();for(unsigned vector:{28u,32u,88u,89u,90u,91u})for(unsigned at=0;at<count;++at)run(at,vector);}
 // Live current references are sampled after admission at each packing child;
 // original current selection and the existing outer caller frame remain live.
 for(auto kind:{K::MultiSerialize,K::PackAll,K::PackOne}){cfg=Config{};cfg.kind=kind;cfg.livePointer=true;run();}
 for(unsigned kind=0;kind<17;++kind){cfg=Config{};cfg.kind=K(kind);cfg.liveCaller=true;run();}
 for(auto kind:{K::ReadProvidedName,K::WriteProvidedName})for(uint32_t alias:{0x1733bu,0x1733cu,0x1733du}){cfg=Config{};cfg.kind=kind;cfg.source=alias;run();}
 for(unsigned kind=0;kind<17;++kind){if(kind==0||kind==10||kind==13)continue;cfg=Config{};cfg.kind=K(kind);cfg.source=kind==9||kind==12?0xffef59:kind>=9?0xffeff0:0xffef1c;run();}
 // Rejected admissions expose no native continuation or observable changes.
 for(unsigned kind=0;kind<17;++kind)for(unsigned variant=0;variant<10;++variant){
  foreground.resetMetrics();b->pc=roots[kind];b->setSP(0xfff800);b->indent=1;b->cycles=1000;b->ccr=0xf5;b->exr=0xa0;b->lastread=0x12345;b->lastwrite=0x23456;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;b->pending_irqs=0x123;b->pending_irqs_high=0x456;
  switch(variant){case 0:b->nativeFetchClocksOnly=false;break;case 1:b->nativeLowBase=nullptr;break;case 2:b->nativeLowSize=0x10000;break;case 3:b->interruptMode2=false;break;case 4:b->indent=0;break;case 5:b->indent=-1;break;case 6:b->setSP(0xfff801);break;case 7:b->setSP(0xfff3fe);break;case 8:b->setSP(0xfffbfe);break;case 9:b->pc=0x71bc;break;}
  std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;const auto pc=b->pc;const auto depth=b->indent;y.events.clear();track=true;const bool handled=foreground.step(*b);track=false;
  check(!handled&&!foreground.active()&&!foreground.started&&!foreground.completed&&!foreground.phases&&!foreground.components&&y.events.empty(),"Store guard claimed ownership/resources");
  check(b->pc==pc&&b->indent==depth&&b->cycles==1000&&b->ccr==0xf5&&b->exr==0xa0&&b->lastread==0x12345&&b->lastwrite==0x23456&&b->pending_irqs==0x123&&b->pending_irqs_high==0x456,"Store guard modified caller");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Store guard modified register");++guards;
 }
 // Resource ownership excludes code/MMIO, CPU scratch and windows crossing
 // its first byte. Test both pointers read from state and scalar caller inputs
 // at the lowest admitted caller SP, with no resource access on rejection.
 const auto reject=[&](K kind,uint32_t pointer,unsigned role,unsigned group=1,unsigned descriptor=6){
  foreground.resetMetrics();b->pc=roots[unsigned(kind)];b->setSP(0xfff400);b->indent=1;b->cycles=1000;b->ccr=0xf5;b->exr=0xa0;b->lastread=0x12345;b->lastwrite=0x23456;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;b->pending_irqs=0x123;b->pending_irqs_high=0x456;
  const auto wide=[&](unsigned a,uint32_t v){for(unsigned n=0;n<4;++n)b->memory[a+n]=uint8_t(v>>(24-8*n));};
  wide(0x167bc,0x30000);wide(0x1748e,0x34000);b->regs[0].er=0x33000;b->regs[4].er=0x30000;b->regs[2].rl=int8_t(group);
  for(unsigned n=0;n<24;++n)b->memory[71306+n]=rom.data()[71306+n];
  if(role==0)wide(0x167bc,pointer);else if(role==1)b->regs[4].er=int32_t(pointer);else if(role==2)wide(0x167bc,pointer-151);else if(role==3)wide(0x1748e,pointer);else if(role==4)b->regs[0].er=int32_t(pointer);
  if(descriptor<6)wide(71306+4*descriptor,pointer);
  std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;std::array<uint8_t,0xc00>scratchBefore{};std::copy_n(b->memory+0xfff000,scratchBefore.size(),scratchBefore.begin());
  y.events.clear();track=true;const bool handled=foreground.step(*b);track=false;
  check(!handled&&!foreground.active()&&!foreground.started&&!foreground.completed&&!foreground.phases&&!foreground.components&&!foreground.initial&&y.events.empty(),"Resource guard claimed store graph");
  check(b->pc==roots[unsigned(kind)]&&b->indent==1&&b->cycles==1000&&b->ccr==0xf5&&b->exr==0xa0&&b->lastread==0x12345&&b->lastwrite==0x23456&&b->pending_irqs==0x123&&b->pending_irqs_high==0x456,"Resource guard changed caller state");
  for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Resource guard changed caller register");check(std::equal(scratchBefore.begin(),scratchBefore.end(),b->memory+0xfff000),"Resource guard changed CPU scratch");++guards;
 };
 for(unsigned kind=1;kind<17;++kind){if(kind==10||kind==13)continue;const unsigned role=kind<=2?0:kind<=8?1:kind==9||kind==12?2:kind==15||kind==16?3:4;
  for(uint32_t pointer:{0u,0x687eu,0xb800u,0x1fffffu,0xbfffffu,0x40000au,0x60000au,0xffff8cu,0xffefffu,0xfff000u,0xfff3fcu,0xfffbffu,0xfffff0u,0xfffffff0u})reject(K(kind),pointer,role);
 }
 for(unsigned group=0;group<256;++group)if(group<1||group>6)reject(K::PackOne,0,5,group);
 for(auto kind:{K::MultiSerialize,K::PackAll,K::PackOne})for(unsigned n=0;n<6;++n)for(uint32_t address:{0u,0x687eu,0x40000au,0xfffff000u})reject(kind,address,5,1,n);
 check(components==0x7ff&&kindMask==0x1ffff&&guards==688&&!allocations&&!b->interpretedSteps,"Store coverage/allocation/interpreter differs");
 std::cout<<"PASS: "<<cases<<" CPU-free store preparation/name calls; "<<phases<<" independent H8 phases; "<<interrupts<<" IRQ preemptions; "<<metricResets<<" metric resets; "<<liveCases<<" evolving resource cases; "<<guards<<" guards; all17 kinds/11 components, exact bytes/time/state/final ABI, zero allocations/interpreter/internal stack writes\n";return 0;
}catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
