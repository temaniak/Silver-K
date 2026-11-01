#include "ksStateReloadService.h"
#include "ksControllerState.h"
#include <cstdlib>
#include <new>
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <tuple>
#include <vector>
#include <stdexcept>
namespace {bool tracking=false;uint64_t allocations=0;}
void* operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
using Event=std::tuple<bool,uint32_t,uint8_t,uint64_t>;
struct Original:H8SEmulator{using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
struct Bus:H8SDevice{
 std::vector<Event> events;
 uint8_t read(uint32_t a)override{const auto v=a==0x400004||a==0x600004?uint8_t(255):state->memory[a];events.emplace_back(false,a,v,state->cycles);return v;}
 void write(uint32_t a,uint8_t v)override{state->memory[a]=v;events.emplace_back(true,a,v,state->cycles);}
};
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::vector<Event> resource(const std::vector<Event>&all,uint32_t pc,H8SDisassembler&d){
 d.pc=d.makepc(pc);d.step();const unsigned length=unsigned(d.getPC())-pc;check(length>=2&&length<=10,"Bad reference decode");
 std::vector<Event> out;unsigned fetch=0;
 for(const auto&e:all){const auto a=std::get<1>(e);if(!std::get<0>(e)&&a==pc+fetch&&fetch<length){++fetch;continue;}if(a>=0xfff000&&a<0xfffc00)continue;out.push_back(e);}
 check(fetch==length,"Reference fetch shape differs");return out;
}
int main(int argc,char**argv){try{
 if(argc!=2)return 77;check(!ks::controllerReferenceAvailable(),"Test linked synchronized H8 reference adapter");ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto native=std::make_unique<std::array<uint8_t,1<<24>>();auto decoder=std::make_unique<H8SDisassembler>();Bus bus;bus.events.reserve(512);a->memmap(&bus,0,1<<24);decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);g_dasm=false;
 uint64_t phases=0,rejects=0;unsigned coverage=0;
 for(unsigned seed=0;seed<32;++seed){
  const unsigned bank=seed%4;const uint32_t fixtureBase=0x845800 + 1024*bank;
  std::fill(a->memory,a->memory+(1<<24),uint8_t(0));for(unsigned base:{0u,0xa00000u})a->loadmem(rom.data().data(),unsigned(rom.data().size()),base);
  const auto byte=[&](uint32_t at,uint8_t v){a->memory[at]=v;};const auto word=[&](uint32_t at,uint16_t v){byte(at,uint8_t(v>>8));byte(at+1,uint8_t(v));};const auto wide=[&](uint32_t at,uint32_t v){word(at,uint16_t(v>>16));word(at+2,uint16_t(v));};
  for(unsigned part=0;part<4;++part){for(unsigned n=0;n<256;++n)byte(fixtureBase+256*part+n,uint8_t(seed*17+n*31));byte(fixtureBase+256*part+187,1);byte(fixtureBase+256*part+188,0);}
  byte(fixtureBase+189,15);byte(fixtureBase+190,15);byte(0x167ab,uint8_t(bank));byte(0x167ba,1);byte(0x167bb,255);byte(0x174a6,1);byte(0x167d0,15);byte(0x167d1,15);byte(0x169fc + 227,0);
  byte(0x158f2,uint8_t(seed));wide(0x167bc,0x169fc);wide(0x167c0,0x16f12);wide(0xfff800,0x4338);
  a->setSP(0xfff800);a->pc=a->makepc(0x6ff4);a->indent=1;a->cycles=1000;a->lastread=-1;a->lastwrite=-1;a->interruptMode2=true;a->exr=0;a->ccr=0;
  std::copy(a->memory,a->memory+(1<<24),native->begin());ks::StateReloadService task;
  const auto validHandler=[](uint8_t category,uint32_t raw){switch(category){case 0:return raw==0x4f4a;case 2:return raw==0x51d8;case 3:return raw==0x51f4;case 4:return raw==0x5252;case 6:return raw==0x51fe;default:return false;}};
  check(ks::StateReloadService::admittedSchema([&](uint32_t at){return (*native)[at];},validHandler),"Normal firmware reload schema rejected");
  uint64_t cycles=1000;int previousWrite=-1;
  unsigned step=0;std::vector<Event> actual;actual.reserve(512);
  while(!task.complete()){
   if(seed>=16&&step%17==0)for(unsigned part=0;part<4;++part)for(unsigned n=0;n<209;++n)if(n<187||n>190){const auto at=fixtureBase+256*part+n;const auto v=uint8_t(seed*19+step*29+n*37);a->memory[at]=(*native)[at]=v;}
   const auto pc=unsigned(a->getPC());const auto cost=task.nextClocks(previousWrite);check(cost,"Reload coordinator deadlocked");actual.clear();
   const auto component=task.component();const auto beforeEffects=task.effect();const auto beforeCoordinator=task.coordinatorComponents(),beforeVoice=task.voiceComponents(),beforePanel=task.panelComponents();unsigned callbacks=0;tracking=true;const auto rejected=task.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},validHandler,previousWrite);tracking=false;check(!rejected&&!callbacks&&task.component()==component&&task.coordinatorComponents()==beforeCoordinator&&task.voiceComponents()==beforeVoice&&task.panelComponents()==beforePanel&&task.effect().read==beforeEffects.read&&task.effect().write==beforeEffects.write&&task.nextClocks(previousWrite)==cost,"Rejected reload budget changed resources/continuation");++rejects;
   tracking=true;const auto used=task.advanceBefore(cost,[&](uint32_t at,unsigned offset){check(at<0xfff000||at>=0xfffc00,"Reload observed internal CPU scratch");const auto v=at==0x400004||at==0x600004?uint8_t(255):(*native)[at];actual.emplace_back(false,at,v,cycles+offset);return v;},[&](uint32_t at,uint8_t v,unsigned offset){check(at<0xfff000||at>=0xfffc00,"Reload wrote internal CPU scratch");(*native)[at]=v;actual.emplace_back(true,at,v,cycles+offset);previousWrite=at<0x400000||(at>=0xfffd10&&at<0xffffa0)?int(at):-1;},validHandler,previousWrite);
   tracking=false;check(used==cost,"Reload budget mismatch");if(task.effect().write)previousWrite=-1;cycles+=cost;bus.events.clear();a->step();const auto wanted=resource(bus.events,pc,*decoder);
   if(cycles!=a->cycles||actual!=wanted){std::cerr<<"seed="<<seed<<" step="<<step<<" component="<<unsigned(task.component())<<" pc="<<std::hex<<pc<<std::dec<<" clocks="<<a->cycles<<'/'<<cycles<<" events="<<wanted.size()<<'/'<<actual.size()<<'\n';for(const auto&rows:{wanted,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<std::dec<<':'<<std::get<3>(e)<<' ';std::cerr<<'\n';}throw std::runtime_error("Reload byte/value/order/time differs");}
   check(++step<100000,"Reload phase bound exceeded");++phases;
  }
  const auto* presentation=task.panel();check(presentation,"Missing native panel graph");
  const auto bounds=presentation->bounds();
  check(uint32_t(a->regs[0].er)==presentation->current(),"Reload final current observation differs");
  check(uint32_t(a->regs[2].er)==((presentation->lastResolvedBase()&0xffff0000u)|bounds),"Reload final bounds/base observation differs");
  check(uint32_t(a->regs[3].er)==((presentation->lastBitSelectorReference()&0xffff0000u)|bounds),"Reload final selector observation differs");
  check(uint32_t(a->regs[4].er)==((task.coordinator().expandedCurrentBase()&0xffff0000u)|0x8046u),"Reload final expanded data/transport address differs");
  check(a->getPC()==0x4338&&a->getSP()==0xfff804,"Reload reference return differs");coverage|=task.coordinatorComponents();check(std::all_of(native->begin()+0xfff000,native->begin()+0xfff800,[](uint8_t v){return !v;})&&std::all_of(native->begin()+0xfff804,native->begin()+0xfffc00,[](uint8_t v){return !v;}),"Reload altered internal CPU frame storage");
  check(std::equal(a->memory,a->memory+0xfff000,native->begin())&&std::equal(a->memory+0xfffc00,a->memory+(1<<24),native->begin()+0xfffc00),"Reload final resource state differs");
 }
 check(!allocations&&coverage==0xfff,"Reload allocation/component contract differs");std::cout<<"PASS: 32 fully native reloads including evolving-source and bank variants / "<<phases<<" independently timed H8 phases / "<<rejects<<" rejected insufficient budgets; mask="<<std::hex<<coverage<<"; closed native graph and final ABI observations checked against independent stock H8; zero reference shims/native allocations/internal CPU frame accesses\n";return 0;
 }catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
