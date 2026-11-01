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
 ks::LiveMidiController state(*b);ks::RuntimeController runtime(*b,state);std::mt19937 rng(0x41444349);
 struct Config{unsigned remaining=0,group=0,raw=0x8000,previous=0x8000,delta=0,armed=0,curve=0,mapping=0,last=255,producer=0x1fe;bool all=false,live=false,fullTimer=false;}cfg;
 uint64_t cases=0,boundaries=0,nested=0,resets=0,mutations=0;uint32_t components=0;
 auto verify=[&](unsigned injectAt=~0u,unsigned flags=0xa5){
  check(!runtime.activeNativeInterrupts(),"Previous native interrupt did not retire");runtime.resetMetrics();
  std::array<int32_t,8> saved{};for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());
  a->setSP(0xfff800);b->setSP(0xfff800);saved[7]=int32_t(0xfff800);
  for(auto*h:{a.get()}){h->pc=h->makepc(0x4338);h->ccr=uint8_t(flags);h->exr=0xa0;h->cycles=1000;h->indent=0;h->lastread=h->lastwrite=-1;h->interruptMode2=true;h->pending_irqs=1ull<<28;h->pending_irqs_high=0;for(auto&p:h->interruptPriority)p=0;h->interruptPriority[28]=1;h->interruptPriority[32]=6;}
  b->pc=0x4338;b->ccr=uint8_t(flags);b->exr=0xa0;b->cycles=1000;b->indent=0;b->lastread=b->lastwrite=-1;b->interruptMode2=true;b->pending_irqs=1ull<<28;b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[28]=1;b->interruptPriority[32]=6;
  const auto byte=[&](uint32_t address,uint8_t value){a->memory[address]=b->memory[address]=value;};
  const auto word=[&](uint32_t address,uint16_t value){byte(address,uint8_t(value>>8));byte(address+1,uint8_t(value));};
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));
   std::fill(m+0x15406,m+0x15800,uint8_t(0));
   for(unsigned n=0;n<256;++n)m[0xb918+n]=uint8_t(cfg.mapping?0x80:n&0x7f);
   for(unsigned n=0;n<260;++n)m[0xb93c+n]=uint8_t(cfg.curve);
   m[0xffec14]=cfg.fullTimer?0:1;m[0xffee70]=128;m[0xffec10]=m[0xffec11]=m[0xffec12]=0;m[0xffec13]=7;
   for(unsigned n=0;n<4;++n){m[0xffee7a+n]=0xff;m[0xffee7e + n]=uint8_t(n==3);m[0xffee82+n]=0;m[0xffee86+n]=uint8_t(n==3);}
   m[0xffec23]=1;m[0xffee97]=2;m[0xffee9a]=255;m[0xffff51]=0;m[0xffff55]=0xff;m[0xffff54]=12;m[0xffff59]=0xe0;m[0xffff50]=31;
   m[0xffee76]=m[0xffee77]=m[0xffee78]=m[0xffee79]=255;m[0xffed6c]=m[0xffed6e]=0;m[0xffed6d]=45;m[0xffed6f]=17;
   for(unsigned n=0;n<8;++n){m[0xb960+n*2]=0xaa;m[0xb961+n*2]=0xaa;}
   for(unsigned n=0;n<64;++n){auto at=0xb970+4*n;m[at]=3;m[at+1]=uint8_t(n+36);m[at+2]=0xff;m[at+3]=0xe2;}
   for(unsigned n=0;n<4;++n){m[0xffec16+n]=uint8_t(1u<<n);m[0xffec24+n]=uint8_t(128u>>n);}
  }
  const auto offset=uint8_t(cfg.group<<1);const auto sample=uint16_t((cfg.raw>>1)+(cfg.previous>>1));
  for(unsigned n=0;n<4;++n){word(0xffff90+2*n,uint16_t(cfg.raw));word(0x15652+offset+2*n,uint16_t(cfg.previous));word(0x1560a+offset+2*n,uint16_t(sample+(n==0||cfg.all?cfg.delta:0)));byte(0x1569a+offset+2*n,uint8_t(cfg.armed));}
  word(0x15406,uint16_t(cfg.producer));word(0x15408,0);byte(0x156e2,uint8_t(cfg.group));byte(0x156e3,uint8_t(cfg.remaining));byte(0x156e4,uint8_t(cfg.last));byte(0xffff98,0xf3);
  unsigned count=0,liveMask=0,scaleReads=0;bool injected=false;
  do{
   if(count==injectAt){a->pending_irqs=b->pending_irqs=1ull<<32;injected=true;}
   if(cfg.live){
    const auto once=[&](unsigned bit,uint32_t pc,auto&& change){if(!(liveMask&(1u<<bit))&&a->getPC()==pc){change();liveMask|=1u<<bit;++mutations;}};
    once(0,0x1664,[&]{byte(0x156e3,17);}); // Remaining was already sampled as zero.
    once(1,0x16c0,[&]{byte(0xffff90,0x12);byte(0xffff91,0x35);word(0x15652,0x4567);}); // Retain high; sample low/history live.
    once(2,0x1720,[&]{const auto address=uint32_t(a->regs[0].er)&0xffffff;word(address+72,0x7777);word(address,0xffff);});
    once(3,0x1726,[&]{byte((uint32_t(a->regs[0].er)+144)&0xffffff,1);});
    once(4,0x1756,[&]{word(uint32_t(a->regs[0].er)&0xffffff,0x2345);});
    if(a->getPC()==0x175c&&scaleReads<2){byte(0x156e2,scaleReads?0:32);++scaleReads;++mutations;}
    once(5,0x17aa,[&]{byte(uint32_t(a->regs[4].er)&0xffffff,0);}); // Curve was sampled before this change.
    once(6,0x1854,[&]{byte(0xb918+uint8_t(a->regs[4].rl),7);});
    once(7,0x185e,[&]{word(0x15406,0x1ff);});
    once(8,0x1864,[&]{word(0x15406,44);}); // Retain the sampled odd queue cursor.
    once(9,0x1818,[&]{byte(0x156e2,28);});
    once(10,0x164c,[&]{byte(0x156e2,127);}); // Mux retains its sampled group 32.
   }
   if(count==3){runtime.resetMetrics();++resets;}
   x.events.clear();y.events.clear();track=true;runtime.step(*b);track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"boundary="<<count<<" remaining="<<cfg.remaining<<" group="<<cfg.group<<" delta="<<cfg.delta<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("ADC phase clocks differ");}
   auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);
   if(expected!=actual){std::cerr<<"boundary="<<count<<" remaining="<<cfg.remaining<<" group="<<cfg.group<<" delta="<<cfg.delta<<" old/new transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto& rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}std::cerr<<"old/new PC="<<std::hex<<a->getPC()<<'/'<<b->getPC()<<std::dec<<'\n';throw std::runtime_error("ADC byte transfer/value/order/time differs");}
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"ADC IRQ admission/priority differs");
   check(std::equal(a->memory+0xffdc00,a->memory+0xfff700,b->memory+0xffdc00),"ADC evolving resource memory differs");
   check(b->pc==0x4338&&b->getSP()==0xfff800&&b->ccr==uint8_t(flags),"Native ADC used CPU continuation");
   for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Native ADC used CPU registers");
   for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Native ADC wrote CPU stack");
   check(++count<8192,"ADC IRQ did not finish");++boundaries;
  }while(a->getPC()!=0x4338||a->indent!=0);
  check(!runtime.activeNativeInterrupts(),"Native ADC left an active continuation");
  check(runtime.nativeInterruptStarts+runtime.nativeInterruptInitial==runtime.nativeInterruptReturns,"Native ADC metric reset lost continuation");
  for(unsigned n=0;n<8;++n)check(a->regs[n].er==b->regs[n].er,"Returned ADC register differs");
  check(a->ccr==b->ccr&&a->exr==b->exr&&a->indent==b->indent&&a->lastread==b->lastread&&a->lastwrite==b->lastwrite,"Returned ADC flags/bus history differ");
  if(injected||cfg.live)check(std::equal(a->memory,a->memory+0xfff700,b->memory)&&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),"ADC full resource memory differs");
  if(cfg.live)check(liveMask==0x7ff&&scaleReads==2,"ADC live resource mutation coverage incomplete");
  components|=runtime.nativeAdcComponents;++cases;if(injected)++nested;return count;
 };
 for(unsigned delta=0;delta<65536;++delta)for(unsigned armed:{0u,1u}){cfg=Config{};cfg.delta=delta;cfg.armed=armed;verify(~0u,delta&255);}
 for(unsigned remaining=1;remaining<256;++remaining){cfg=Config{};cfg.remaining=remaining;cfg.group=32;verify();}
 for(unsigned group:{0u,1u,28u,31u,32u,33u,36u,72u,127u,128u,252u,255u})for(unsigned delta:{0u,1u,511u,512u,2047u,2048u,32767u,32768u,65535u})for(unsigned curve:{0u,1u}){
  cfg=Config{};cfg.group=group;cfg.delta=delta;cfg.curve=curve;cfg.all=true;verify();
 }
 for(unsigned raw:{0u,1u,511u,512u,513u,1023u,1024u,0x7fffu,0x8000u,0x8400u,0xfe00u,0xff00u,0xffffu})for(unsigned group:{0u,32u})for(unsigned curve:{0u,1u})for(unsigned producer:{0u,1u,0x1feu,0x1ffu})for(unsigned mapping:{0u,1u}){
  cfg=Config{};cfg.raw=raw;cfg.previous=raw^1;cfg.group=group;cfg.curve=curve;cfg.producer=producer;cfg.mapping=mapping;cfg.delta=0x4000;cfg.all=true;verify();
 }
 for(unsigned group:{0u,32u}){
  cfg=Config{};cfg.group=group;cfg.delta=0x4000;cfg.all=true;cfg.curve=1;const auto phases=verify();
  cfg.fullTimer=true;for(unsigned at=1;at<phases;++at)verify(at);
 }
 cfg=Config{};cfg.delta=0x4000;cfg.all=true;cfg.curve=1;cfg.live=true;verify();
 check(components==0x3f,"ADC complete child graph coverage incomplete");check(!allocations&&!b->interpretedSteps,"Native ADC allocated or interpreted");
 std::cout<<"PASS: "<<cases<<" complete CPU-free ADC calls; "<<boundaries<<" independent H8 phases; "<<nested<<" full TPU0 preemptions; components="<<std::hex<<components<<std::dec<<"; "<<resets<<" metric resets; "<<mutations<<" live resource changes; all byte transfers/timestamps/resource memory and caller state exact; no allocations/interpreter/CPU stack writes\n";
 return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
