#include "ksRuntimeController.h"
#include "ksStateReloadPanelService.h"
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
 std::vector<Access> events;bool overlay=false;unsigned dspBusy=0,dspReads=0,txWait=0,txReads=0;
 uint8_t read(uint32_t a)override{auto v=this->state->memory[overlay&&a>=0xb8e8&&a<0x200000?0xa00000+a:a];if(a==0x600004&&dspReads++<dspBusy)v=0;if(a==0xffed6f&&txReads++<txWait)v=uint8_t(this->state->memory[0xffed6d]+1);events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}
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
 if(argc!=2)return 77;check(!ks::controllerReferenceAvailable(),"Synchronized H8 adapter linked");ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();for(unsigned base:{0u,0xa00000u}){a->loadmem(rom.data().data(),unsigned(rom.data().size()),base);b->loadmem(rom.data().data(),unsigned(rom.data().size()),base);}decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);g_dasm=false;
 Bus<H8SDevice>x;Bus<ks::ControllerBus>y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;ks::LiveMidiController state(*b);ks::RuntimeController runtime(*b,state);std::mt19937 rng(0x50414e4c);
 using T=ks::StateReloadPanelService;uint64_t calls=0,phases=0,interrupts=0,liveChanges=0,guards=0;uint32_t components=0;unsigned busy=0;uint32_t base=0x24000;bool overlay=false;
 const auto validate=[](uint8_t category,uint32_t raw){switch(category){case 0:return raw==0x4f4a;case 2:return raw==0x51d8;case 3:return raw==0x51f4;case 4:return raw==0x5252;case 6:return raw==0x51fe;default:return false;}};
 auto verify=[&](unsigned seed,unsigned flags,unsigned inject=~0u,unsigned vector=32,bool live=false){
  runtime.resetMetrics();T service;std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());a->setSP(0xfff800);b->setSP(0xfff800);saved[7]=int32_t(0xfff800);a->pc=a->makepc(0x6c3c);b->pc=0x6c3c;a->cycles=b->cycles=1000;a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0xa0;a->indent=b->indent=1;a->lastread=b->lastread=0x12345;a->lastwrite=b->lastwrite=0x23456;a->interruptMode2=b->interruptMode2=true;a->pending_irqs=a->pending_irqs_high=b->pending_irqs=b->pending_irqs_high=0;for(auto&p:a->interruptPriority)p=0;for(auto&p:b->interruptPriority)p=0;a->interruptPriority[vector]=b->interruptPriority[vector]=6;x.overlay=y.overlay=overlay;x.dspBusy=y.dspBusy=busy;x.dspReads=y.dspReads=0;b->nativeLowBase=overlay?b->memory+0xa00000:b->memory;
  const auto byte=[&](uint32_t at,uint8_t v){at&=0xffffff;a->memory[at]=b->memory[at]=v;if(overlay&&at>=0xb8e8&&at<0x200000)a->memory[0xa00000 + at]=b->memory[0xa00000 + at]=v;};const auto word=[&](uint32_t at,uint16_t v){byte(at,uint8_t(v>>8));byte(at+1,uint8_t(v));};const auto longValue=[&](uint32_t at,uint32_t v){for(unsigned n=0;n<4;++n)byte(at+n,uint8_t(v>>(24-8*n)));};
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



  for(unsigned n=0;n<4;++n){word(0xffff90 + 2*n,0x8000);word(0x15652 + 2*n,0x8000);word(0x1560a + 2*n,0);byte(0x1569a + 2*n,0);}byte(0x156e2,36);byte(0x156e3,0);byte(0x156e4,255);byte(0xffff98,0x73);for(unsigned at:{0x1537cu,0x1537eu,0x15406u,0x15408u,0x156e8u,0x156eau})word(at,0);
  for(unsigned n=0;n<1024;++n)byte(base+n,uint8_t(seed+17*n));longValue(0x167bc,base);longValue(0x167c0,0x25000);longValue(0x167a6,0);byte(base+222,uint8_t(seed));byte(base+225,uint8_t(seed>>1));word(0x159f2,0x1234);byte(0x600004,7);byte(0xffec22,7);byte(0xffff62,255);for(unsigned at:{0x167d2u,0x167d3u,0x167bau,0x16784u,0x1679eu,0x167a1u,0x167a3u,0x16790u})byte(at,uint8_t(seed+at));longValue(0xfff800,0x4338);
  check(T::admittedSchema([&](uint32_t at){return b->memory[at];},validate),"Authenticated panel schema rejected");unsigned count=0,liveMask=0;bool injected=false;
  do{
   if(count==inject){if(vector<64)a->pending_irqs=b->pending_irqs=1ull<<vector;else a->pending_irqs_high=b->pending_irqs_high=1ull<<(vector-64);injected=true;}
   if(live){const auto once=[&](unsigned bit,uint32_t pc,auto&&change){if(!(liveMask&(1u<<bit))&&a->getPC()==pc){change();liveMask|=1u<<bit;++liveChanges;}};once(0,0x52f0,[&]{longValue(0x167bc,base+1);});once(1,0x5218,[&]{byte(base+225,3);});once(2,0x52c0,[&]{byte(base+222,2);});once(3,0x6c76,[&]{byte(base+89,255);});once(4,0x6c9c,[&]{byte(0x167d2,0x55);});once(5,0x6cfc,[&]{byte(0xfff803,0x3a);});}
   x.events.clear();y.events.clear();track=true;
   const bool nativeIrq=runtime.activeNativeInterrupts()||ks::InterruptScheduler::select(b->pending_irqs,b->pending_irqs_high,b->interruptPriority,b->exr).vector>=0;if(nativeIrq)runtime.step(*b);
   else {const auto start=b->cycles;const auto cost=service.nextClocks(b->lastwrite);const auto used=service.advanceBefore(cost,[&](uint32_t at,unsigned offset){b->cycles=start+offset;b->lastread=at<0x400000||(at>=0xfffd10&&at<0xffffa0)?int(at):-1;return y.read(at);},[&](uint32_t at,uint8_t value,unsigned offset){b->cycles=start+offset;b->lastwrite=at<0x400000||(at>=0xfffd10&&at<0xffffa0)?int(at):-1;y.write(at,value);},validate,b->lastwrite);check(cost&&used==cost,"Panel phase duration invalid");if(service.effect().write)b->lastwrite=-1;if(service.effect().read)b->lastread=-1;
    if(service.complete()){for(unsigned n=0;n<4;++n){b->cycles=start+8+3*n;b->lastread=-1;(void)y.read(0xfff800 + n);}}b->cycles=start+cost;}
   track=false;a->step();if(a->cycles!=b->cycles){std::cerr<<"seed="<<seed<<" phase="<<count<<" pc="<<std::hex<<a->getPC()<<" component="<<unsigned(service.component())<<std::dec<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("Panel clocks differ");}
   const auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);if(expected!=actual){std::cerr<<"seed="<<seed<<" phase="<<count<<" pc="<<std::hex<<a->getPC()<<" component="<<unsigned(service.component())<<std::dec<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Panel resources differ");}
   check(a->exr==b->exr&&a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high,"Panel IRQ state differs");if(!nativeIrq&&(a->lastwrite!=b->lastwrite||(service.complete()&&a->lastread!=b->lastread))){std::cerr<<"seed="<<seed<<" phase="<<count<<" pc="<<std::hex<<a->getPC()<<" component="<<unsigned(service.component())<<" read="<<a->lastread<<'/'<<b->lastread<<" write="<<a->lastwrite<<'/'<<b->lastwrite<<std::dec<<'\n';throw std::runtime_error("Panel resource history differs");}check(b->pc==0x6c3c&&b->getSP()==0xfff800&&b->ccr==flags,"Panel changed external CPU fields");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Panel changed CPU scratch");for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Panel wrote CPU stack");check(++count<1000000,"Panel stalled");++phases;
  }while(a->getPC()!=(live?0x433a:0x4338)||a->indent);
  check(service.complete(),"Panel service incomplete at H8 return");check(uint32_t(a->regs[0].er)==service.current(),"Panel final observed current differs");check(uint32_t(a->regs[2].er)==((service.lastResolvedBase()&0xffff0000)|service.bounds()),"Panel final resolved base/bounds differs");check(uint32_t(a->regs[3].er)==((service.lastBitSelectorReference()&0xffff0000)|service.bounds()),"Panel final selector/bounds differs");check(uint16_t(a->regs[4].r)==0x8046,"Panel final Auxiliary cache address differs");check(runtime.nativeInterruptStarts==runtime.nativeInterruptReturns&&!runtime.activeNativeInterrupts(),"Panel IRQ ownership differs");if(injected||live)check(std::equal(a->memory,a->memory+0xfff700,b->memory)&&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),"Panel full resource state differs");components|=service.components();++calls;if(injected)++interrupts;return count;
 };
 for(unsigned seed=0;seed<256;++seed)verify(seed,seed);for(unsigned flags=0;flags<256;++flags)for(unsigned seed:{0u,1u,127u,255u})verify(seed,flags);
 const auto count=verify(2,0xe3);for(unsigned vector:{28u,32u,88u,89u,90u,91u})for(unsigned at=0;at<count;++at)verify(2,0xe3,at,vector);
 for(bool alias:{false,true}){overlay=alias;for(uint32_t at:{0x24000u,0x24001u,0xa24000u,0xa24001u,0xffdc00u,0xffdc01u}){base=at;verify(255,0xe3,~0u,32,true);}}overlay=false;base=0x24000;
 for(unsigned wait:{1u,3u,65538u}){busy=wait;verify(2,0xe3);}busy=0;
 for(const auto&s:T::schema)for(unsigned n=0;n<8;++n){const auto at=0xfd66 + 8*s.key+n;const auto old=b->memory[at];b->memory[at]^=1;check(!T::admittedSchema([&](uint32_t a){return b->memory[a];},validate),"Mutated panel descriptor admitted");b->memory[at]=old;++guards;}
 for(unsigned category:{0u,2u,3u,4u,6u})for(unsigned n=0;n<4;++n){const auto at=0x1013e + 4*category+n;const auto old=b->memory[at];b->memory[at]^=1;check(!T::admittedSchema([&](uint32_t a){return b->memory[a];},validate),"Mutated panel handler admitted");b->memory[at]=old;++guards;}
 for(uint32_t target:{0xfe96u,0xfe99u,0xfea0u,0xfed8u,0xfd74u,0xfddcu,0x10149u}){T rejected;bool observed=false,failed=false;int previous=-1;try{for(unsigned n=0;n<10000&&!rejected.complete();++n){const auto cost=rejected.nextClocks(previous);rejected.advanceBefore(cost,[&](uint32_t at,unsigned){auto value=y.read(at);if(at==target){value^=1;observed=true;}return value;},[&](uint32_t at,uint8_t value,unsigned){y.write(at,value);previous=at<0x400000||(at>=0xfffd10&&at<0xffffa0)?int(at):-1;},validate,previous);if(rejected.effect().write)previous=-1;}}catch(const std::logic_error&){failed=true;}check(observed&&failed,"Live mutated panel schema was silently used");++guards;}
 check(components==0x7ff&&!allocations&&!b->interpretedSteps,"Panel closure/allocation/interpreter coverage differs");std::cout<<"PASS: "<<calls<<" complete CPU-free PanelReload graphs; "<<phases<<" independent H8 phases; "<<interrupts<<" native IRQ points; "<<liveChanges<<" live changes; "<<guards<<" descriptor/handler guards; all11 components and11fixedmappedkeys; exact byte resources/clocks/history and frozen CPU fields; zero allocations/interpreter/internal CPU stack\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
