#include "ksStateEventsForeground.h"
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
 ks::LiveMidiController state(*b);ks::RuntimeController runtime(*b,state);ks::StateEventsForeground foreground;std::mt19937 rng(0x45564e54);
 using K=ks::StateEventsService::Kind;constexpr std::array<unsigned,9>roots{{0x1412,0x15da,0x21e2,0x1f12,0x20fc,0x2144,0x218c,0x20de,0x20c8}};
 struct Config{K kind=K::ControlPop;unsigned value=0x92,selector=0x22,producer=2,consumer=0,flags=0xa5;
  uint32_t history=0x158f0,coefficients=0x158f4;unsigned minimum=17,maximum=218,offset=5,factor=174,bias=1024;
  int32_t r0=0x12345678;bool live=false,liveCaller=false,negativeR2=false; }cfg;
 uint64_t cases=0,boundaries=0,nested=0,resets=0,mutations=0,guards=0;uint32_t components=0,kinds=0;
 const auto run=[&](unsigned injectAt=~0u,unsigned vector=32){
  check(!foreground.active()&&!runtime.activeNativeInterrupts(),"Previous event graph did not retire");runtime.resetMetrics();foreground.resetMetrics();
  const auto root=roots[unsigned(cfg.kind)];std::array<int32_t,8>saved{};
  for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());
  a->regs[0].er=b->regs[0].er=cfg.r0;
  a->regs[1].er=b->regs[1].er=int32_t(cfg.history);
  a->regs[2].er=b->regs[2].er=int32_t(cfg.coefficients|(cfg.negativeR2?0x80000000u:0));
  a->regs[3].r=b->regs[3].r=int16_t(cfg.bias);
  a->regs[6].r=b->regs[6].r=int16_t((cfg.value<<8)|cfg.selector);
  a->setSP(0xfff800);b->setSP(0xfff800);for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;
  a->pc=a->makepc(root);a->ccr=uint8_t(cfg.flags);a->exr=0xa0;a->cycles=1000;a->indent=1;a->lastread=0x12345;a->lastwrite=0x23456;a->interruptMode2=true;a->pending_irqs=a->pending_irqs_high=0;
  b->pc=root;b->ccr=uint8_t(cfg.flags);b->exr=0xa0;b->cycles=1000;b->indent=1;b->lastread=0x12345;b->lastwrite=0x23456;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;
  for(auto&p:a->interruptPriority)p=0;for(auto&p:b->interruptPriority)p=0;a->interruptPriority[vector]=b->interruptPriority[vector]=6;
  const auto byte=[&](uint32_t at,uint8_t value){a->memory[at]=b->memory[at]=value;};
  const auto word=[&](uint32_t at,uint16_t value){byte(at,uint8_t(value>>8));byte(at+1,uint8_t(value));};
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));std::fill(m+0x1537c,m+0x15800,uint8_t(0x7b));
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
  for(unsigned n=0;n<4;++n){word(0xffff90 + 2*n,0x8000);word(0x15652 + 2*n,0x8000);word(0x1560a + 2*n,0);byte(0x1569a + 2*n,0);}
  byte(0x156e2,0);byte(0x156e3,0);byte(0x156e4,255);byte(0xffff98,0x73);
  for(unsigned index:{0x1537cu,0x15406u,0x156e8u}){word(index,uint16_t(cfg.producer));word(index+2,uint16_t(cfg.consumer));word(index+4+cfg.consumer,uint16_t((cfg.value<<8)|cfg.selector));}
  byte(cfg.history&0xffffff,uint8_t(cfg.minimum));byte((cfg.history+1)&0xffffff,uint8_t(cfg.maximum));
  byte(cfg.coefficients&0xffffff,uint8_t(cfg.offset));byte((cfg.coefficients+1)&0xffffff,uint8_t(cfg.factor));
  for(auto at:{0x158ecu,0x158eeu}){byte(at,uint8_t(cfg.offset));byte(at+1,uint8_t(cfg.factor));}
  word(0xfff800,0);word(0xfff802,0x4338);uint32_t target=0x4338;unsigned count=0,liveMask=0;bool injected=false;
  do{
   const auto pc=uint32_t(a->getPC());
   if(count==injectAt){if(vector<64)a->pending_irqs=b->pending_irqs=1ull<<vector;else a->pending_irqs_high=b->pending_irqs_high=1ull<<(vector-64);injected=true;}
   if(cfg.live){
    const auto once=[&](unsigned bit,uint32_t at,auto&&change){if(!(liveMask&(1u<<bit))&&pc==at){change();liveMask|=1u<<bit;++mutations;}};
    once(0,0x15e4,[&]{word(0x15406,8);});
    once(1,0x1610,[&]{word(0x1540a + cfg.consumer,0xe422);});
    once(2,0x2148,[&]{byte(0x158ed,137);});
    once(3,0x2150,[&]{byte(0x158ec,23);});
   }
   if(cfg.liveCaller&&count==5){byte(0xfff803,0x3a);target=0x433a;++mutations;}
   if(count==3){runtime.resetMetrics();foreground.resetMetrics();++resets;}
   x.events.clear();y.events.clear();track=true;
   if(runtime.activeNativeInterrupts()||b->pending_irqs||b->pending_irqs_high)runtime.step(*b);else check(foreground.step(*b),"Native event graph declined owned phase");
   track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"kind="<<unsigned(cfg.kind)<<" PC="<<std::hex<<pc<<std::dec<<" phase="<<count<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("Event phase clocks differ");}
   const auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);
   if(expected!=actual){std::cerr<<"kind="<<unsigned(cfg.kind)<<" PC="<<std::hex<<pc<<std::dec<<" phase="<<count<<" transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Event resource byte/value/order/clock differs");}
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"Event IRQ admission/priority differs");
   check(std::equal(a->memory+0xffdc00,a->memory+0xfff700,b->memory+0xffdc00),"Evolving internal resources differ");
   check(std::equal(a->memory+0x1537c,a->memory+0x15900,b->memory+0x1537c),"Evolving event/curve resources differ");
   if(foreground.active()||runtime.activeNativeInterrupts()){
    check(b->pc==root&&b->getSP()==0xfff800&&b->ccr==uint8_t(cfg.flags),"Event graph used CPU continuation");
    for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Event graph used CPU registers");
   }
   for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Event graph wrote CPU scratch stack");
   check(++count<8192,"Event graph did not finish");++boundaries;
  }while(a->getPC()!=target||a->indent!=0);
  check(!foreground.active()&&!runtime.activeNativeInterrupts(),"Event graph retained continuation");
  check(foreground.initial+foreground.started==foreground.completed,"Metric reset lost event continuation");
  check(runtime.nativeInterruptInitial+runtime.nativeInterruptStarts==runtime.nativeInterruptReturns,"Event IRQ continuation metric differs");
  for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"kind="<<unsigned(cfg.kind)<<" reg="<<n<<" old/new="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("Event caller register differs");}
  if(a->ccr!=b->ccr){std::cerr<<"kind="<<unsigned(cfg.kind)<<" old/newCCR="<<std::hex<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<" entry="<<cfg.flags<<std::dec<<" value/selector="<<cfg.value<<'/'<<cfg.selector<<" min/max="<<cfg.minimum<<'/'<<cfg.maximum<<" bias="<<cfg.bias<<'\n';throw std::runtime_error("Event caller CCR differs");}
  check(a->lastread==b->lastread&&a->lastwrite==b->lastwrite&&a->exr==b->exr&&a->indent==b->indent&&b->getSP()==0xfff804,"Event caller return/bus history differs");
  if(injected||cfg.live||cfg.liveCaller)check(std::equal(a->memory,a->memory+0xfff700,b->memory)&&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),"Event full resource memory differs");
  if(cfg.live)check(liveMask==15,"Event live resource phases incomplete");
  components|=foreground.components;kinds|=1u<<unsigned(cfg.kind);++cases;if(injected)++nested;return count;
 };
 // Exhaustive values/selector for the complete conversion graph and its queue parent.
 for(auto kind:{K::ControlPop,K::Convert})for(unsigned selector=0;selector<256;++selector)for(unsigned value=0;value<256;++value){cfg=Config{};cfg.kind=kind;cfg.selector=selector;cfg.value=value;cfg.flags=value;cfg.offset=(value*37+selector)&255;cfg.factor=(selector*73+value)&255;run();}
 // All queue offsets, including odd events and wrapping consumer publications.
 for(unsigned kind=0;kind<4;++kind)for(unsigned consumer=0;consumer<(kind==0?128u:512u);++consumer)for(bool empty:{false,true}){cfg=Config{};cfg.kind=K(kind);cfg.consumer=consumer;cfg.producer=empty?consumer:(consumer+2)&(kind==0?126:510);cfg.value=consumer&255;cfg.selector=(consumer*73)&255;run();}
 for(unsigned minimum=0;minimum<256;++minimum)for(unsigned maximum=0;maximum<256;++maximum){cfg=Config{};cfg.kind=K::Coefficients;cfg.minimum=minimum;cfg.maximum=maximum;cfg.bias=(minimum*251+maximum*769)&65535;cfg.flags=maximum;run();}
 for(auto kind:{K::Scale,K::Range,K::Square})for(unsigned value=0;value<256;++value)for(unsigned edge:{0u,1u,127u,128u,255u}){cfg=Config{};cfg.kind=kind;cfg.value=value;cfg.minimum=edge;cfg.maximum=255-edge;cfg.offset=edge;cfg.factor=edge;cfg.bias=uint16_t(value*251+edge*257);cfg.flags=value;run();}
 // All initial flags, preserved caller values and saved signed projections.
 for(unsigned kind=0;kind<9;++kind)for(unsigned flags=0;flags<256;++flags)for(int32_t saved:{0,1,-1,int32_t(0x80000000u),int32_t(0x7fffffffu)}){cfg=Config{};cfg.kind=K(kind);cfg.flags=flags;cfg.r0=saved;cfg.negativeR2=flags&1;cfg.value=uint8_t(saved);cfg.selector=uint8_t(flags);run();}
 for(auto kind:{K::Coefficients,K::Range,K::Scale})for(auto base:{0x158f0u,0x158f1u,0xa158f0u,0xa158f1u,0xffef00u,0xffef01u})for(unsigned edge:{0u,1u,127u,128u,255u}){cfg=Config{};cfg.kind=kind;cfg.history=base;cfg.coefficients=base+4;cfg.minimum=edge;cfg.maximum=255-edge;cfg.value=edge;cfg.bias=edge*257;run();}
 // The last complete peripheral pair stays admitted below the CPU scratch
 // boundary, for each independent history/coefficient descriptor role.
 for(unsigned role=0;role<4;++role){cfg=Config{};cfg.kind=role==0?K::Scale:role==3?K::Range:K::Coefficients;
  cfg.history=role==1||role==3?0xffeffe:0xffefe0;cfg.coefficients=role==0||role==2?0xffeffe:0xffefe4;run();}
 // Interrupt each original phase of each root, including every child variant.
 for(unsigned kind=0;kind<9;++kind)for(unsigned selector:kind==1||kind==4?std::initializer_list<unsigned>{0x20,0x22,0x23,0x7f}:std::initializer_list<unsigned>{0x22}){cfg=Config{};cfg.kind=K(kind);cfg.selector=selector;const auto count=run();for(unsigned vector:{28u,32u,88u,89u,90u,91u})for(unsigned at=0;at<count;++at)run(at,vector);}
 cfg=Config{};cfg.live=true;run();cfg=Config{};cfg.liveCaller=true;run();
 // Admission must leave diagnostic ownership and all resources untouched.
 for(unsigned kind=0;kind<9;++kind)for(unsigned variant=0;variant<12;++variant){
  foreground.resetMetrics();b->pc=roots[kind];b->setSP(0xfff800);b->indent=1;b->cycles=1000;b->ccr=0xf5;b->exr=0xa0;b->lastread=0x12345;b->lastwrite=0x23456;
  b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->interruptMode2=true;b->nativeLowSize=0x200000;b->pending_irqs=0x123;b->pending_irqs_high=0x456;b->regs[1].er=0x158f0;b->regs[2].er=0x158f4;
  const auto setWord=[&](uint32_t at,uint16_t value){b->memory[at]=uint8_t(value>>8);b->memory[at+1]=uint8_t(value);};
  for(auto at:{0x1537cu,0x15406u,0x156e8u}){setWord(at,2);setWord(at+2,0);}
  switch(variant){case 0:b->nativeFetchClocksOnly=false;break;case 1:b->nativeLowBase=nullptr;break;case 2:b->interruptMode2=false;break;case 3:b->indent=0;break;case 4:b->indent=-1;break;case 5:b->setSP(0xfff801);break;case 6:b->setSP(0xfff3fe);break;case 7:b->setSP(0xfffbfe);break;case 8:b->pc=0x1414;break;
  case 9:if(kind<4)setWord(kind==0?0x1537c:kind==3?0x156e8:0x15406,512);else if(kind==5||kind==7)b->regs[2].er=0x2144;else if(kind==8)b->regs[1].er=0xffff8c;else b->setSP(0xa18000);break;
  case 10:if(kind<4)setWord((kind==0?0x1537c:kind==3?0x156e8:0x15406)+2,512);else if(kind==5||kind==7)b->regs[2].er=0xffff8c;else if(kind==8)b->regs[1].er=0x20c8;else b->setSP(0xa18000);break;
  case 11:if(kind<4)b->nativeLowSize=0x10000;else if(kind==7)b->regs[1].er=0xfff800;else b->setSP(0xa18000);break;}
  std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;const auto pc=b->pc;const auto depth=b->indent;
  y.events.clear();track=true;const auto handled=foreground.step(*b);track=false;
  check(!handled&&!foreground.active()&&!foreground.started&&!foreground.phases&&!foreground.components,"Diagnostic guard claimed event graph");
  check(b->pc==pc&&b->indent==depth&&b->cycles==1000&&b->ccr==0xf5&&b->exr==0xa0&&b->lastread==0x12345&&b->lastwrite==0x23456&&b->pending_irqs==0x123&&b->pending_irqs_high==0x456&&y.events.empty(),"Rejected event changed caller/resource state");
  for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Rejected event changed registers");++guards;
 }
 // With the lowest admitted SP, the original Scale pushL would overwrite
 // fff3fc..fff3ff. No descriptor pair may intersect any reserved CPU scratch
 // byte, including an odd pair starting one byte below fff000. Check every
 // start and each descriptor role without claiming or modifying the caller.
 for(unsigned role=0;role<4;++role)for(uint32_t pointer=0xffefff;pointer<0xfffc00;++pointer){
  const auto kind=role==0?K::Scale:role==3?K::Range:K::Coefficients;foreground.resetMetrics();
  b->pc=roots[unsigned(kind)];b->setSP(0xfff400);b->indent=1;b->cycles=1000;b->ccr=0xf5;b->exr=0xa0;b->lastread=0x12345;b->lastwrite=0x23456;
  b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->interruptMode2=true;b->nativeLowSize=0x200000;b->pending_irqs=0x123;b->pending_irqs_high=0x456;
  b->regs[1].er=int32_t(role==1||role==3?pointer:0x158f0);b->regs[2].er=int32_t(role==0||role==2?pointer:0x158f4);
  std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;
  std::array<uint8_t,0xc00>scratchBefore{};std::copy_n(b->memory+0xfff000,scratchBefore.size(),scratchBefore.begin());
  y.events.clear();track=true;const auto handled=foreground.step(*b);track=false;
  check(!handled&&!foreground.active()&&!foreground.started&&!foreground.completed&&!foreground.phases&&!foreground.components&&!foreground.initial,"CPU scratch alias guard claimed event graph");
  check(std::all_of(foreground.kinds.begin(),foreground.kinds.end(),[](uint64_t count){return !count;}),"CPU scratch alias guard changed kind metrics");
  check(b->pc==roots[unsigned(kind)]&&b->indent==1&&b->cycles==1000&&b->ccr==0xf5&&b->exr==0xa0&&b->lastread==0x12345&&b->lastwrite==0x23456&&b->pending_irqs==0x123&&b->pending_irqs_high==0x456&&y.events.empty(),"Rejected CPU scratch alias changed caller/resources");
  for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Rejected CPU scratch alias changed registers");
  check(std::equal(scratchBefore.begin(),scratchBefore.end(),b->memory+0xfff000),"Rejected CPU scratch alias changed scratch RAM");++guards;
 }
 check(components==255&&kinds==511&&guards==12400&&!allocations&&!b->interpretedSteps,"Event graph coverage/allocation/interpreter checks failed");
 std::cout<<"PASS: "<<cases<<" CPU-free State event calls; "<<boundaries<<" independent H8 phases; "<<nested<<" native IRQ preemptions; "<<resets<<" metric resets; "<<mutations<<" live resource changes; "<<guards<<" diagnostic guards; all nine kinds/eight components, exact bytes/clocks/resources and final caller ABI; zero allocations/interpreter/internal CPU stack writes\n";return 0;
}catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
