#include "ksSysExOutputService.h"
#include "ksStateSaveAbi.h"
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
#include <tuple>
#include <vector>
namespace{bool tracking=false;uint64_t allocations=0;void check(bool b,const char*m){if(!b)throw std::logic_error(m);}}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {
using T=ks::SysExOutputService;using Event=std::tuple<bool,uint32_t,uint8_t,uint64_t>;
struct Original final:H8SEmulator{using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
struct OracleBus final:H8SDevice{
 std::vector<Event>events;uint64_t ready=0,waits=0;
 OracleBus(){events.reserve(32);}
 uint8_t read(uint32_t a)override{auto v=uint8_t(state->memory[a]);if(a==0xffff8c){if(state->cycles<ready){++waits;v&=127;}else v|=128;}events.emplace_back(false,a,v,state->cycles);return v;}
 void write(uint32_t a,uint8_t v)override{state->memory[a]=v;events.emplace_back(true,a,v,state->cycles);if(a==0xffff8b)ready=state->cycles+200;}
};
struct NativeBus{
 std::vector<uint8_t>memory=std::vector<uint8_t>(1<<24);std::array<Event,8>events{};unsigned size=0;uint64_t start=0,ready=0,waits=0;int previous=-1;
 uint8_t read(uint32_t a,unsigned offset){a&=0xffffff;check(size<8,"Native output event capacity");auto v=memory[a];if(a==0xffff8c){if(start+offset<ready){++waits;v&=127;}else v|=128;}events[size++]={false,a,v,start+offset};return v;}
 void write(uint32_t a,uint8_t v,unsigned offset){a&=0xffffff;check(size<8,"Native output event capacity");memory[a]=v;events[size++]={true,a,v,start+offset};previous=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;if(a==0xffff8b)ready=start+offset+200;}
};
bool scratch(uint32_t a){return a>=0xfff000&&a<0xfffc00;}
}
int main(int argc,char**argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid KS OS");auto a=std::make_unique<Original>();auto n=std::make_unique<NativeBus>();auto decoder=std::make_unique<H8SDisassembler>();OracleBus bus;a->memmap(&bus,0,1<<24);
 a->loadmem(rom.data().data(),unsigned(rom.data().size()),0);decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);g_dasm=false;
 std::mt19937 rng(0x53584f55);uint64_t graphs=0,phases=0,waits=0,reads=0,writes=0;uint32_t components=0;constexpr std::array<uint32_t,5>roots{{0xac6c,0xac88,0xac96,0xa924,0xfe4}};
 auto run=[&](unsigned kind,unsigned seed,uint16_t count,uint32_t source){
  a->cycles=0;a->pc=a->makepc(roots[kind]);a->ccr=uint8_t(seed*17);a->exr=uint8_t(seed&7);a->indent=1;a->pending_irqs=0;
  for(auto&r:a->regs)r.er=int32_t(rng());a->regs[6].er=int32_t(source);a->regs[5].r=int16_t(count);a->regs[0].rh=int8_t(seed);a->setSP(0xfff800);
  for(unsigned i=0;i<4;++i)a->memory[0xfff800+i]=uint8_t(0x600000u>>(24-8*i));
  a->lastread=a->lastwrite=-1;bus.ready=100+seed%7;bus.waits=0;
  for(unsigned i=0;i<12;++i)a->memory[0x16044+i]=uint8_t(seed+9*i);
  if(kind==1)for(unsigned i=0;i<(count?unsigned(count):65536);++i)a->memory[(source+i)&0xffffff]=uint8_t(seed+13*i);
  a->memory[0xffff8c]=uint8_t(seed&127);a->memory[0xffed68]=0x77;a->memory[0xffec64]=0xaa;a->memory[0xffec65]=0xbb;a->memory[0xffec66]=0xcc;a->memory[0xffec67]=0xdd;
  std::memcpy(n->memory.data(),a->memory,1<<24);n->ready=bus.ready;n->waits=0;n->previous=-1;
  ks::StateSaveCallerAbi abi;for(unsigned i=0;i<7;++i)abi.values[i]=uint32_t(a->regs[i].er);abi.flags=a->ccr;
  T service(T::Kind(kind),{source,count,uint8_t(seed)});uint64_t end=0,steps=0;
  while(!service.complete()){
   const auto cost=service.nextClocks(n->previous);check(cost,"Missing native output phase");end+=cost;n->start=end-cost;n->size=0;
   tracking=true;const auto used=service.advanceBefore(cost,[&](uint32_t at,unsigned dt){return n->read(at,dt);},[&](uint32_t at,uint8_t v,unsigned dt){n->write(at,v,dt);},0,n->previous);tracking=false;
   check(used==cost,"Native output phase deadline");if(service.effect().write)n->previous=-1;
   std::array<Event,8>actual{};unsigned actualCount=0;
   while(a->cycles<end){
    const auto pc=unsigned(a->getPC());check(pc!=0x600000,"H8 returned before native output");decoder->pc=decoder->makepc(pc);decoder->step();const auto length=unsigned(decoder->getPC())-pc;check(length>=2&&length<=10,"H8 instruction size");
    bus.events.clear();a->step();
    for(const auto&e:bus.events){const auto at=std::get<1>(e);if(scratch(at)||(!std::get<0>(e)&&at>=pc&&at<pc+length))continue;check(actualCount<8,"H8 resource event capacity");actual[actualCount++]=e;}
   }
   if(a->cycles!=end||actualCount!=n->size){std::cerr<<"kind="<<kind<<" phase="<<steps<<" nativeEnd="<<end<<" h8End="<<a->cycles<<" nativeEvents="<<n->size<<" h8Events="<<actualCount<<'\n';throw std::logic_error("H8 output phase/cycle resource count differs");}
   for(unsigned i=0;i<actualCount;++i){if(actual[i]!=n->events[i]){std::cerr<<"kind="<<kind<<" phase="<<steps<<" H8="<<std::hex<<std::get<1>(actual[i])<<'/'<<std::get<3>(actual[i])<<" native="<<std::get<1>(n->events[i])<<'/'<<std::get<3>(n->events[i])<<std::dec<<'\n';throw std::logic_error("H8 output transfer value/order/time differs");}if(std::get<0>(actual[i]))++writes;else++reads;}
   ++steps;++phases;check(steps<4000000,"H8 output graph stalled");
  }
  check(unsigned(a->getPC())==0x600000&&a->getSP()==0xfff804,"H8 output caller return");tracking=true;ks::projectSysExOutput(abi,service);tracking=false;
  for(unsigned i=0;i<7;++i)if(abi.values[i]!=uint32_t(a->regs[i].er)){std::cerr<<"kind="<<kind<<" register="<<i<<" H8="<<std::hex<<uint32_t(a->regs[i].er)<<" native="<<abi.values[i]<<std::dec<<'\n';throw std::logic_error("H8 output final-only value projection differs");}
  if(abi.flags!=a->ccr){std::cerr<<"kind="<<kind<<" H8flags="<<unsigned(a->ccr)<<" nativeFlags="<<unsigned(abi.flags)<<'\n';throw std::logic_error("H8 output final-only flags differ");}
  check(a->exr==uint8_t(seed&7),"H8 output priority restore");
  for(auto span:{std::pair<uint32_t,unsigned>{0x16044,12},{0xffec64,4},{0xffed68,1},{0xffff88,8}})check(!std::memcmp(a->memory+span.first,n->memory.data()+span.first,span.second),"H8 output resource result differs");
  check(bus.waits==n->waits,"H8/native ready polling differs");waits+=n->waits;components|=service.components();++graphs;
 };
 for(unsigned kind=0;kind<5;++kind)for(unsigned seed=0;seed<32;++seed)run(kind,seed,kind==1?uint16_t(1+seed%17):12,0x30000);
 for(auto count:{uint16_t(255),uint16_t(256),uint16_t(1024),uint16_t(0)})run(1,37,count,0x81ff00);
 check(!allocations&&waits&&components==15,"H8 output coverage/allocation contract");
 std::cout<<"PASS H8 SysEx output graphs="<<graphs<<" phases="<<phases<<" reads="<<reads<<" writes="<<writes<<" waits="<<waits<<" components="<<components<<" allocations="<<allocations<<'\n';return 0;
 }catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
