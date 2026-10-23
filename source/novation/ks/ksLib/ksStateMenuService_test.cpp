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
 std::vector<Access> events;bool overlay=false;
 uint8_t read(uint32_t a)override{auto v=this->state->memory[overlay&&a>=0xb8e8&&a<0x200000?0xa00000+a:a];events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}
 void write(uint32_t a,uint8_t v)override{this->state->memory[overlay&&a>=0xb8e8&&a<0x200000?0xa00000+a:a]=v;events.emplace_back(true,a,v,this->state->cycles,uint32_t(this->state->getPC()));}
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
 ks::LiveMidiController state(*b);ks::RuntimeController runtime(*b,state);auto& foreground=runtime.stateMenuForeground;std::mt19937 rng(0x4d454e55);
 const std::array<unsigned,14>roots{{0x4528,0x453a,0x46ac,0x46da,0x46ec,0x471a,0x4efe,0x4134,0x12fa,0x5a46,0x5a7c,0x52ac,0x52e6,0x52fc}};
 const std::array<unsigned,14>returns{{0,0,0,0,0,0,0x4f04,0x414c,0x1304,0x5a7a,0x5a8e,0x52e2,0x52fa,0x530c}};
 struct Config{unsigned page=0,special=0,edit=1,contextKind=126,screen=9,selector=1,index=2,offset=37,interval=400,action=0;uint32_t descriptor=0x18000,base=0x24000,clock=0x12345678;bool overlay=false;}cfg;
 uint64_t cases=0,boundaries=0,nested=0,resets=0,mutations=0,guardCases=0;uint32_t components=0;
 auto verify=[&](unsigned kind,unsigned savedValue,unsigned flags,unsigned injectAt=~0u,unsigned vector=32,bool live=false){
  check(!foreground.active()&&!runtime.activeNativeInterrupts(),"Previous menu graph did not retire");runtime.resetMetrics();foreground.resetMetrics();
  std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());
  a->regs[0].er=b->regs[0].er=int32_t(savedValue);a->regs[6].r=b->regs[6].r=int16_t(cfg.interval);
  if(kind==9)a->regs[1].er=b->regs[1].er=int32_t(cfg.index);
  if(kind>=11)a->regs[0].er=b->regs[0].er=int32_t(cfg.descriptor);
  if(kind==7)a->regs[5].er=b->regs[5].er=cfg.action?0x471a:0x46da;
  for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;
  a->setSP(0xfff800);b->setSP(0xfff800);saved[7]=int32_t(0xfff800);
  a->pc=a->makepc(roots[kind]);a->ccr=uint8_t(flags);a->exr=0xa0;a->cycles=1000;a->indent=1;a->lastread=0x12345;a->lastwrite=0x23456;a->interruptMode2=true;a->pending_irqs=a->pending_irqs_high=0;for(auto&p:a->interruptPriority)p=0;a->interruptPriority[vector]=6;
  b->pc=roots[kind];b->ccr=uint8_t(flags);b->exr=0xa0;b->cycles=1000;b->indent=1;b->lastread=0x12345;b->lastwrite=0x23456;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[vector]=6;
  x.overlay=y.overlay=cfg.overlay;b->nativeLowBase=cfg.overlay?b->memory+0xa00000:b->memory;
  const auto byte=[&](uint32_t address,uint8_t value){a->memory[address]=b->memory[address]=value;if(cfg.overlay&&address>=0xb8e8&&address<0x200000)a->memory[0xa00000+address]=b->memory[0xa00000+address]=value;};
  const auto word=[&](uint32_t address,uint16_t value){byte(address,uint8_t(value>>8));byte(address+1,uint8_t(value));};
  const auto longValue=[&](uint32_t address,uint32_t value){for(unsigned n=0;n<4;++n)byte(address+n,uint8_t(value>>(24-8*n)));};
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
  byte(0x156e2,36);byte(0x156e3,0);byte(0x156e4,255);byte(0xffff98,0x73);
  for(unsigned at:{0x1537cu,0x1537eu,0x15406u,0x15408u,0x156e8u,0x156eau})word(at,0);
  byte(0x16790,uint8_t(cfg.page));byte(0x1679e,uint8_t(cfg.special));byte(0x16784,uint8_t(cfg.edit));byte(0x167a1,uint8_t(cfg.offset));byte(0x16785,uint8_t(cfg.screen));
  longValue(0x167bc,cfg.base);longValue(0x167a6,0xdeadbeef);byte(71586+cfg.index,uint8_t(cfg.contextKind));longValue(0xffec10,cfg.clock);
  byte((cfg.descriptor+2)&0xffffff,uint8_t(cfg.selector));byte((cfg.base+cfg.selector)&0xffffff,uint8_t(cfg.index));byte((cfg.descriptor+cfg.index+3)&0xffffff,uint8_t(cfg.offset));byte((cfg.descriptor+3)&0xffffff,uint8_t(cfg.offset));
  longValue(0x16770,0x7b7b7b7b);longValue(0x16774,0x7b7b7b7b);longValue(0xfff800,0x4338);
  uint32_t target=live?0x433a:0x4338;const bool pressed=!(flags&4);const int finalDepth=kind<6?1:0;
  if(kind==0)target=pressed?0x6c3c:0x4862;else if(kind==1)target=pressed?0xb83c:0x4862;
  else if(kind==3||kind==5)target=kind==3?0x58e4:0x58ce;
  else if(kind==2||kind==4)target=!pressed?0x5eb0:cfg.page==2?(kind==2?0x75a0:0x7548):cfg.special?(kind==2?0xb60c:0xb5e4):!cfg.edit?0x5db2:kind==2?0x58e4:0x58ce;
  unsigned count=0,liveMask=0;bool injected=false;
  do{
   if(count==injectAt){if(vector<64)a->pending_irqs=b->pending_irqs=1ull<<vector;else a->pending_irqs_high=b->pending_irqs_high=1ull<<(vector-64);injected=true;}
   if(live){
    const auto once=[&](unsigned bit,uint32_t pc,auto&&change){if(!(liveMask&(1u<<bit))&&a->getPC()==pc){change();liveMask|=1u<<bit;++mutations;}};
    if(kind==0)once(0,0x4532,[&]{byte(0x167a1,0x80);});
    if(kind==2||kind==4){once(0,kind==2?0x46b0:0x46f0,[&]{byte(0x16790,1);});once(1,0x4efe,[&]{byte(0x16784,255);});}
    if(kind==7||kind==8||kind==2||kind==3||kind==4||kind==5)once(2,0x1300,[&]{longValue(0xffec10,0xffff8001);});
    if(kind==6)once(0,0x4efe,[&]{byte(0x16784,128);});
    if(kind==9){once(0,0x5a52,[&]{byte(71586+cfg.index,125);});once(1,0x5a5e,[&]{byte(0x16785,5);});}
    if(kind==11){once(0,0x52c4,[&]{byte(cfg.base+cfg.selector,7);byte(cfg.descriptor+10,0x80);});once(1,0x52cc,[&]{byte(cfg.descriptor+10,0x81);});once(2,0x52d8,[&]{longValue(0x167bc,0x26001);});}
    if(kind==12)once(0,0x52f0,[&]{longValue(0x167bc,0x26001);});
    if(kind>=6)once(7,returns[kind],[&]{byte(0xfff803,0x3a);});
   }
   if(count==1){runtime.resetMetrics();foreground.resetMetrics();++resets;}
   x.events.clear();y.events.clear();track=true;runtime.step(*b);track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"kind="<<kind<<" boundary="<<count<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("Menu phase clocks differ");}
   auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);
   if(expected!=actual){std::cerr<<"kind="<<kind<<" boundary="<<count<<" saved="<<savedValue<<" flags="<<flags<<" transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}std::cerr<<"old/new PC="<<std::hex<<a->getPC()<<'/'<<b->getPC()<<std::dec<<'\n';throw std::runtime_error("Menu byte transfer/value/order/time differs");}
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"Menu IRQ admission/priority differs");
   if(b->pc!=target||b->indent!=finalDepth){check(b->pc==roots[kind]&&b->getSP()==0xfff800&&b->ccr==uint8_t(flags),"Native menu used CPU continuation");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Native menu used CPU registers");}
   for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Native menu wrote CPU stack");
   check(++count<8192,"Menu graph did not finish");++boundaries;
  }while(a->getPC()!=target||a->indent!=finalDepth);
  check(!foreground.active()&&!runtime.activeNativeInterrupts(),"Native menu left active continuation");check(foreground.started+foreground.initialActive==foreground.completed,"Menu metric reset lost continuation");
  check(runtime.nativeInterruptStarts+runtime.nativeInterruptInitial==runtime.nativeInterruptReturns,"Menu nested IRQ metric balance differs");
  for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"kind="<<kind<<" register="<<n<<" old/new="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("Returned menu register differs");}
  if(a->ccr!=b->ccr||a->exr!=b->exr||a->indent!=b->indent||a->lastread!=b->lastread||a->lastwrite!=b->lastwrite){std::cerr<<"kind="<<kind<<" flags="<<std::hex<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<" history="<<a->lastread<<'/'<<b->lastread<<':'<<a->lastwrite<<'/'<<b->lastwrite<<std::dec<<'\n';throw std::runtime_error("Returned menu flags/history differ");}
  check(b->getSP()==unsigned(kind<6?0xfff800:0xfff804),"Menu outer caller ownership differs");
  check(std::equal(a->memory+0x16770,a->memory+0x16778,b->memory+0x16770)&&std::equal(a->memory+0x167a6,a->memory+0x167aa,b->memory+0x167a6)&&a->memory[0x167a1]==b->memory[0x167a1],"Menu persistent resources differ");
  if(injected||live)check(std::equal(a->memory,a->memory+0xfff700,b->memory)&&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),"Menu full evolving memory differs");
  if(live&&kind>=6)check(liveMask&128,"Menu live outer caller was not observed");components|=foreground.components;++cases;if(injected)++nested;return count;
 };
 for(unsigned kind=0;kind<14;++kind)for(unsigned flags=0;flags<256;++flags)for(unsigned value:{0u,1u,0x7fffu,0x8000u,0xffffu,0x7fffffffu,0x80000000u,0xffffffffu}){cfg=Config{};cfg.action=flags&1;cfg.clock=flags*0x01010101u;cfg.interval=value;verify(kind,value,flags);}
 for(unsigned kind:{2u,4u})for(unsigned page:{0u,1u,2u,9u,255u})for(unsigned special=0;special<256;++special)for(unsigned edit:{0u,1u,128u,255u}){cfg=Config{};cfg.page=page;cfg.special=special;cfg.edit=edit;verify(kind,special,0xe3);}
 for(unsigned type=0;type<256;++type)for(unsigned screen=0;screen<256;++screen){cfg=Config{};cfg.contextKind=type;cfg.screen=screen;cfg.index=(type+screen)&255;verify(9,type*257,screen);}
 for(uint32_t descriptor:{0x18000u,0x18001u,0xa18000u,0xa18001u,0xffdc00u,0xffdc01u})for(unsigned selector=0;selector<256;++selector)for(unsigned index:{0u,1u,127u,128u,255u})for(unsigned kind:{11u,12u,13u}){cfg=Config{};cfg.descriptor=descriptor;cfg.selector=selector;cfg.index=index;cfg.offset=(selector+index)&255;verify(kind,index*257,(selector+index)&255);}
 for(uint32_t clock:{0u,1u,0x7fffu,0x8000u,0xffffu,0x10000u,0x7fffffffu,0x80000000u,0xffffffffu})for(unsigned kind:{2u,3u,4u,5u,7u,8u})for(unsigned flags:{0u,1u,0x20u,0xe3u}){cfg=Config{};cfg.clock=clock;verify(kind,0x8a5c,flags);}
 for(uint32_t base:{0x24000u,0x24001u,0xa24000u,0xa24001u,0xffdc00u,0xffdc01u,0x80024000u,0xff024001u})for(unsigned kind:{11u,12u,13u})for(unsigned offset:{0u,1u,127u,255u}){cfg=Config{};cfg.base=base;cfg.offset=offset;verify(kind,0,0xe3);}
 for(bool overlay:{false,true})for(unsigned kind=0;kind<14;++kind){cfg=Config{};cfg.overlay=overlay;verify(kind,0x80000000,0xe3,~0u,32,true);}
 for(unsigned kind=0;kind<14;++kind){cfg=Config{};const auto count=verify(kind,0x8a5c,0xe3);for(unsigned vector:{28u,32u,88u,89u,90u,91u})for(unsigned at=0;at<count;++at)verify(kind,0x8a5c,0xe3,at,vector);}
 for(unsigned kind=0;kind<14;++kind){cfg=Config{};verify(kind,0x8a5c,0xe3,~0u,32,true);}
 // Reject diagnostic caller frames and any data span that could cross the
 // reserved CPU scratch, including the odd FFEFFF->FFF000 pair.
 for(unsigned kind=0;kind<14;++kind)for(unsigned variant=0;variant<10;++variant){
  foreground.resetMetrics();b->pc=roots[kind];b->setSP(0xfff800);b->indent=1;b->cycles=1000;b->ccr=0xf5;b->exr=0xa0;b->lastread=0x12345;b->lastwrite=0x23456;b->nativeLowSize=0x200000;
  b->pending_irqs=0x123;b->pending_irqs_high=0x456;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->interruptMode2=true;
  b->regs[0].er=0x18000;b->regs[1].er=0;b->regs[5].er=0x46da;for(unsigned n=0;n<4;++n)b->memory[0x167bc + n]=uint8_t(0x24000u>>(24-8*n));
  switch(variant){case 0:b->nativeFetchClocksOnly=false;break;case 1:b->nativeLowBase=nullptr;break;case 2:b->interruptMode2=false;break;case 3:b->indent=0;break;case 4:b->indent=-1;break;case 5:b->setSP(0xfff801);break;case 6:b->setSP(0xfff3fe);break;case 7:b->setSP(0xfffbfe);break;case 8:b->setSP(0xa18000);break;case 9:b->pc=0x4698;break;}
  std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;const auto pc=b->pc;const auto depth=b->indent;const auto cycles=b->cycles;
  y.events.clear();track=true;const auto handled=foreground.step(*b);track=false;
  check(!handled&&!foreground.active()&&!foreground.started&&!foreground.completed&&!foreground.phases&&!foreground.components,"Rejected menu admission claimed continuation");
  check(b->pc==pc&&b->indent==depth&&b->cycles==cycles&&b->ccr==0xf5&&b->exr==0xa0&&b->lastread==0x12345&&b->lastwrite==0x23456,"Rejected menu admission changed caller state");
  check(b->pending_irqs==0x123&&b->pending_irqs_high==0x456&&y.events.empty(),"Rejected menu admission changed resources/IRQs");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Rejected menu admission changed CPU registers");++guardCases;
 }
 for(unsigned kind:{7u,9u,11u,12u,13u})for(unsigned variant=0;variant<5;++variant){
  foreground.resetMetrics();b->pc=roots[kind];b->setSP(0xfff800);b->indent=1;b->cycles=1000;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->interruptMode2=true;b->regs[5].er=0x46da;b->regs[0].er=0x18000;b->regs[1].er=0;
  const uint32_t bad=variant==0?0xffefff:variant==1?0xfff000:variant==2?0xfff7ff:variant==3?0xfffbff:0xffffff;
  if(kind==7)b->regs[5].er=int32_t(bad);else if(kind==9)b->regs[1].er=int32_t(bad-71586);else b->regs[0].er=int32_t(bad-2);
  y.events.clear();track=true;const auto handled=foreground.step(*b);track=false;check(!handled&&!foreground.active()&&!foreground.started&&!foreground.phases&&b->cycles==1000&&y.events.empty(),"Unsafe menu input claimed ownership");++guardCases;
 }
 check(guardCases==165&&components==63&&!allocations&&!b->interpretedSteps,"Menu graph guard/closure/allocation/interpreter contract differs");
 std::cout<<"PASS: "<<cases<<" CPU-free menu/edit calls; "<<boundaries<<" independent H8 phases; "<<nested<<" native IRQ preemptions; "<<resets<<" metric resets; "<<mutations<<" live resource changes; "<<guardCases<<" diagnostic admission guards; all fourteen roots, closed navigation/repeat children, context and descriptor-address resolution, exact bytes/clocks/resources/final ABI; zero allocations/interpreter/internal CPU stack writes\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
