#include "ksStateReloadVoiceService.h"
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
 std::vector<Access> events;unsigned busy=0,fault=0,statusReads=0;bool stalled=false;
 uint8_t read(uint32_t a)override{auto v=this->state->memory[a];if(a==0x400004||a==0x600004){++statusReads;v=uint8_t(busy?(--busy,0):stalled?(fault==2?4:0):6);}events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}
 void write(uint32_t a,uint8_t v)override{this->state->memory[a]=v;if(a==0xffec22)stalled=false;events.emplace_back(true,a,v,this->state->cycles,uint32_t(this->state->getPC()));}
};
bool scratch(uint32_t a){return a>=0xfff700&&a<0xfff804;}
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
int main(int argc,char**argv){try{if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid OS");auto a=std::make_unique<Original>();auto decoder=std::make_unique<H8SDisassembler>();auto memory=std::make_unique<std::array<uint8_t,1<<24>>();for(unsigned base:{0u,0xa00000u}){a->loadmem(rom.data().data(),unsigned(rom.data().size()),base);std::copy(rom.data().begin(),rom.data().end(),memory->begin()+base);}decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);g_dasm=false;Bus<H8SDevice>x;x.events.reserve(256);a->memmap(&x,0,1<<24);std::vector<Access>native;native.reserve(256);std::mt19937 rng(0x524c5643);using T=ks::StateReloadVoiceService;using K=T::Kind;
 struct Config{K kind=K::Voices;unsigned count=1,part=0,base=0,age=0x9234,pattern=0,start=0,busy=0,fault=0,flags=0xa5;bool live=false;}cfg;uint64_t cases=0,phases=0,faultCases=0,liveCases=0;uint32_t components=0,kinds=0;
 const auto run=[&]{T task(cfg.kind);const auto root=cfg.kind==K::Voices?0x936e:0xa106;for(unsigned n=0;n<8;++n)a->regs[n].er=int32_t(rng());const auto originalR0=uint32_t(a->regs[0].er),originalR1=uint32_t(a->regs[1].er);a->setSP(0xfff800);a->pc=a->makepc(root);a->indent=1;a->ccr=uint8_t(cfg.flags);a->exr=0;a->cycles=1000;a->lastread=-1;a->lastwrite=-1;a->interruptMode2=true;a->pending_irqs=a->pending_irqs_high=0;for(auto&p:a->interruptPriority)p=0;
  const auto byte=[&](uint32_t p,uint8_t v){a->memory[p]=(*memory)[p]=v;};const auto word=[&](uint32_t p,uint16_t v){byte(p,uint8_t(v>>8));byte(p+1,uint8_t(v));};const auto wide=[&](uint32_t p,uint32_t v){word(p,uint16_t(v>>16));word(p+2,uint16_t(v));};
  for(unsigned n=0;n<256;++n){const auto p=0x17560+20*n;wide(p,0x30000+n*256);byte(p+4,uint8_t(cfg.part+(cfg.pattern==2?1:0)));wide(p+8,uint32_t(cfg.pattern==1?0:cfg.pattern==3&&n&1?0:cfg.age)<<16|0x1234);}for(unsigned n=0;n<16;++n)byte(0x160d2+334*cfg.part+192+n,0xab);byte(0x174a6,uint8_t(cfg.count));byte(0x17526,uint8_t(cfg.part));byte(0x167ba,uint8_t(cfg.base));word(0x159f2,0x1234);byte(0xffec22,0xa0);for(unsigned p:{0x400000u,0x600000u})for(unsigned n=0;n<16;++n)byte(p+n,0);wide(0xfff800,0x4338);x.busy=cfg.busy;x.fault=cfg.fault;x.stalled=cfg.fault!=0;x.statusReads=0;unsigned busy=cfg.busy,nativeReads=0,count=0;bool stalled=cfg.fault!=0,started=false,live=false;uint64_t clocks=1000;int previous=-1;
  do{const auto pc=uint32_t(a->getPC());if(cfg.kind==K::Voices&&!started&&pc==0xa656){byte(0x174a7,uint8_t(cfg.start));started=true;}if(cfg.live&&!live&&pc==0x9394){word(0x17560+20*task.lastSlot()+8,0xfedc);live=true;}const auto cost=task.nextClocks(previous);check(cost,"Owned reload voice continuation missing phase");native.clear();x.events.clear();const auto used=[&]{track=true;const auto result=task.advanceBefore(cost,[&](uint32_t p,unsigned offset){uint8_t v;if(p==0x400004||p==0x600004){++nativeReads;v=uint8_t(busy?(--busy,0):stalled?(cfg.fault==2?4:0):6);}else v=(*memory)[p];native.emplace_back(false,p,v,clocks+offset,0);return v;},[&](uint32_t p,uint8_t v,unsigned offset){(*memory)[p]=v;previous=p<0x400000||(p>=0xfffd10&&p<0xffffa0)?int(p):-1;if(p==0xffec22)stalled=false;native.emplace_back(true,p,v,clocks+offset,0);},previous);track=false;return result;}();check(used==cost,"Reload voice phase admission mismatch");clocks+=used;if(task.effect().write)previous=-1;a->step();
   if(a->cycles!=clocks){std::cerr<<"kind="<<unsigned(cfg.kind)<<" age="<<cfg.age<<" count="<<cfg.count<<" PC="<<std::hex<<pc<<std::dec<<" phase="<<count<<" clocks="<<a->cycles<<'/'<<clocks<<'\n';throw std::runtime_error("Reload voice independent H8 phase clocks differ");}const auto expected=resources(x.events,decoder.get()),actual=resources(native,nullptr);if(expected!=actual){std::cerr<<"kind="<<unsigned(cfg.kind)<<" age="<<cfg.age<<" count="<<cfg.count<<" PC="<<std::hex<<pc<<std::dec<<" phase="<<count<<" transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Reload voice independent H8 resource/value/order/timestamp differs");}check(++count<1000000,"Reload voice graph stalled");++phases;
  }while(uint32_t(a->getPC())!=0x4338||a->indent);
  check(task.complete()&&!task.critical()&&nativeReads==x.statusReads&&!busy&&!x.busy&&!stalled&&!x.stalled,"Reload voice ownership/readiness differs");if(cfg.kind==K::Voices){check(uint32_t(a->regs[3].er)==0x17560u+20*task.lastSlot(),"Reload voice last slot outcome differs");uint32_t r0=(task.releasedAny()?uint32_t(task.dspRetried()?task.dspRemaining():0)<<16:originalR0&0xffff0000)|(originalR0&0xff00);uint32_t r1=(task.releasedAny()?0:originalR1&0xffff0000)|(originalR1&0xff00)|cfg.part;check(uint32_t(a->regs[0].er)==r0&&uint32_t(a->regs[1].er)==r1,"Reload voice final semantic context differs");if(task.releasedAny()){if(!(uint16_t(a->regs[2].e)==task.lastReleaseAge()&&a->regs[2].r==1&&uint32_t(a->regs[5].er)==0x600000&&uint32_t(a->regs[6].er)==task.lastPatch()&&uint16_t(a->regs[4].r)==uint16_t(task.lastAddress()+1)))std::cerr<<"case="<<cases<<" R2="<<std::hex<<a->regs[2].er<<" age="<<task.lastReleaseAge()<<" R4="<<uint16_t(a->regs[4].r)<<" address="<<task.lastAddress()<<" R5="<<a->regs[5].er<<" R6="<<a->regs[6].er<<" patch="<<task.lastPatch()<<std::dec<<"\n";check(uint16_t(a->regs[2].e)==task.lastReleaseAge()&&a->regs[2].r==1&&uint32_t(a->regs[5].er)==0x600000&&uint32_t(a->regs[6].er)==task.lastPatch()&&uint16_t(a->regs[4].r)==uint16_t(task.lastAddress()+1),"Reload voice DSP/patch result projection differs");}const auto flags=uint8_t((cfg.flags&0xd0)|(r0&0x80000000?8:0)|(!r0?4:0));check(a->ccr==flags,"Reload voice final flags projection differs");}
  else{check(uint16_t(a->regs[4].r)==uint16_t(task.lastAddress()+1)&&uint32_t(a->regs[5].er)==0x400000&&a->regs[1].er==0x100,"Reload signal result projection differs");}
  for(unsigned p=0x15000;p<0x30000;++p)check(a->memory[p]==(*memory)[p],"Reload voice persistent RAM differs");for(unsigned p:{0x400000u,0x600000u,0xffec00u,0xffff00u})for(unsigned n=0;n<(p==0xffec00?0x100u:p==0xffff00?0x100u:16u);++n)check(a->memory[p+n]==(*memory)[p+n],"Reload voice high resources differ");if(cfg.fault){check(task.dspFaults()==1,"Complete DSP timeout did not resume");++faultCases;}if(cfg.live){check(live,"Reload voice live fixture missed phase");++liveCases;}components|=task.components();kinds|=1u<<unsigned(cfg.kind);++cases;
 };
 for(unsigned age=0;age<65536;++age){cfg=Config{};cfg.age=age;cfg.start=age&255;run();}for(unsigned part=0;part<256;++part)for(unsigned base=0;base<256;++base){cfg=Config{};cfg.kind=K::Signal;cfg.part=part;cfg.base=base;run();}for(unsigned count:{0u,1u,2u,127u,128u,255u})for(unsigned part:{0u,3u,127u,255u})for(unsigned pattern=0;pattern<4;++pattern){cfg=Config{};cfg.count=count;cfg.part=part;cfg.pattern=pattern;run();}for(unsigned flags=0;flags<256;++flags)for(unsigned pattern=0;pattern<4;++pattern){cfg=Config{};cfg.flags=flags;cfg.pattern=pattern;run();}for(unsigned fault:{1u,2u})for(auto kind:{K::Voices,K::Signal}){cfg=Config{};cfg.kind=kind;cfg.fault=fault;run();}for(unsigned busy:{1u,2u,255u}){cfg=Config{};cfg.busy=busy;run();}cfg=Config{};cfg.live=true;run();check(components==0x3ff&&kinds==3&&faultCases==4&&liveCases==1&&!allocations,"Reload voice coverage/allocation differs");std::cout<<"PASS: "<<cases<<" CPU-independent reload voice/signal graphs; "<<phases<<" stock H8 phases; 4 complete65536retry faults; exact byte/time/RAM/semantic final ABI; no allocations, no ControllerCpu or runtime adapter\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
