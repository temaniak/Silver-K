#include "kstationControllerStandaloneExpressionRuntime.h"
#if defined(__clang__)
#pragma clang attribute push(__attribute__((no_sanitize("signed-integer-overflow"))), apply_to=function)
#endif
#include "h8s/h8s.hpp"
#if defined(__clang__)
#pragma clang attribute pop
#endif
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <new>
#include <vector>
namespace {
bool watch=false;uint64_t allocations=0;
void check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
class Original:public H8SEmulator {public:void histories(int r,int w){lastread=r;lastwrite=w;}};
struct Event{uint64_t clocks;uint32_t address;uint8_t value;bool write;};
template<class S>struct Tape {
 S&state;std::array<Event,32>events{};unsigned count=0,group=0,reads=0,delay=0;bool enabled=false,fault=false,recovered=false;std::array<uint8_t,16>ports{};uint8_t pins=0x82;
 explicit Tape(S&s):state(s){}
 uint8_t readValue(uint32_t a){uint8_t v=state.memory[a];if(a==0x600004){++reads;v=fault&&!recovered?0:reads<=delay?0:uint8_t(group==1?16:7);}else if(a>=0x600000&&a<0x600010)v=ports[a-0x600000];else if(a==0xffffd2)v=pins;else if(a==0xfffff2)v=0;
 if(enabled){check(count<events.size(),"DSP tape capacity");events[count++]={state.cycles,a,v,false};}return v;}
 void writeValue(uint32_t a,uint8_t v){if(enabled){check(count<events.size(),"DSP tape capacity");events[count++]={state.cycles,a,v,true};}if(a>=0x600000&&a<0x600010){ports[a-0x600000]=v;if(a==0x600000&&v==8){group=1;reads=0;}if(a==0x60000e){group=2;reads=0;}}else if(a==0xffffd2){pins=v;recovered=true;}else state.memory[a]=v;}
};
struct OriginalBus:public H8SDevice {Tape<Original>&t;explicit OriginalBus(Tape<Original>&v):t(v){}uint8_t read(uint32_t a)override{return t.readValue(a);}void write(uint32_t a,uint8_t v)override{t.writeValue(a,v);}};
struct NativeBus:public kstation::ControllerResourceBus {Tape<kstation::ControllerDeviceState>&t;explicit NativeBus(Tape<kstation::ControllerDeviceState>&v):t(v){}bool mapped(uint32_t a)const override{return (a>=0xb000&&a<0xd400)||(a>=0xbf98&&a<0xbf9a)||(a>=0xc59a&&a<0xc99a)||(a>=0xbfc0&&a<0xc06c)||(a>=0xd022&&a<0xd0d0)||(a>=0xcbde&&a<0xcbee)||(a>=0xc598&&a<0xc59a)||(a>=0x600000&&a<0x600010)||a>=0xffffa0;}uint8_t read(uint32_t a)override{return t.readValue(a);}void write(uint32_t a,uint8_t v)override{t.writeValue(a,v);}};
struct Fixture {
 std::unique_ptr<Original>a=std::make_unique<Original>();std::unique_ptr<kstation::ControllerDeviceState>b=std::make_unique<kstation::ControllerDeviceState>();Tape<Original>x{*a};Tape<kstation::ControllerDeviceState>y{*b};OriginalBus ob{x};NativeBus nb{y};kstation::ControllerInterruptRuntime irq;kstation::ControllerDspTransferRuntime dsp;kstation::ControllerStandaloneExpressionRuntime modulation;uint64_t cases=0,phases=0,io=0,shorts=0;
 explicit Fixture(const std::vector<uint8_t>&rom){a->loadmem(rom.data(),unsigned(rom.size()),0);b->loadMemory(rom.data(),rom.size(),0);b->loadMemory(rom.data(),rom.size(),0x400000);a->memmap(&ob,0xb000,0x2400);a->memmap(&ob,0xbf98,2);a->memmap(&ob,0xc59a,0x400);a->memmap(&ob,0xbfc0,0xac);a->memmap(&ob,0xd022,0xae);a->memmap(&ob,0xcbde,16);a->memmap(&ob,0xc598,2);a->memmap(&ob,0x600000,16);a->memmap(&ob,0xffffa0,0x60);b->attachBus(&nb);check(irq.bindVerifiedFirmware(*b,rom.data(),rom.size()),"DSP IRQ binding");check(dsp.bindVerifiedFirmware(*b,rom.data(),rom.size()),"DSP resource binding");check(modulation.bindVerifiedFirmware(*b,rom.data(),rom.size()),"modulation binding");}
 void run(unsigned kind,unsigned flags,unsigned delay,bool cacheMatch,bool pending,bool fault){
  dsp.reset();modulation.reset();a->cycles=b->cycles=100;for(auto&v:a->regs)v.er=0;a->reg32(5)=0x600000;a->reg32(6)=0xcf00;for(unsigned i=0;i<256;++i)a->memory[0xcf00+i]=b->memory[0xcf00+i]=uint8_t(i*53+flags*7);for(unsigned i=0;i<4;++i)a->memory[0xbfc0+i]=b->memory[0xbfc0+i]=uint8_t(flags*(i+1));a->memory[0xd024]=b->memory[0xd024]=0x40;a->memory[0xd025]=b->memory[0xd025]=0;a->memory[0xd026]=b->memory[0xd026]=0x41;a->memory[0xd027]=b->memory[0xd027]=0;a->setSP(0xffff00);a->memory[0xffff00]=0;a->memory[0xffff01]=5;a->memory[0xffff02]=0;a->memory[0xffff03]=0;
  const auto address=uint16_t((cases*773)^0x8040);const auto word=uint32_t(cases*1447)^0xabcdef;auto cache=uint16_t(cacheMatch?address:address+1);a->memory[0xc598]=b->memory[0xc598]=uint8_t(cache>>8);a->memory[0xc599]=b->memory[0xc599]=uint8_t(cache);
  a->reg16(4)=int16_t(address);a->reg16(9)=int16_t(word>>8);a->reg8(1)=int8_t(word);a->ccr=uint8_t(flags|0x80);a->exr=0;a->pending_irqs=b->pendingIrqs=pending?(uint64_t(1)<<36):0;a->pending_irqs_high=b->pendingIrqsHigh=0;a->interruptMode2=b->interruptMode2=false;a->histories(-1,-1);b->lastRead=b->lastWrite=-1;irq.setMask(kstation::ControllerInterruptRuntime::Mask::Disabled);
  x.enabled=y.enabled=true;x.count=y.count=0;x.group=y.group=0;x.reads=y.reads=0;x.delay=y.delay=delay;x.fault=y.fault=fault;x.recovered=y.recovered=false;x.ports.fill(0);y.ports.fill(0);x.ports[10]=y.ports[10]=0x12;x.ports[12]=y.ports[12]=0x34;x.ports[14]=y.ports[14]=0x56;x.pins=y.pins=0x82;
  unsigned entry=kind?0x2e18:0x2df0;uint8_t part=uint8_t(flags%3);a->reg8(14)=int8_t(part);a->memory[0xd022]=b->memory[0xd022]=8;for(unsigned j=0;j<8;++j){auto v=0xd030+j*18;for(unsigned q=0;q<18;++q)a->memory[v+q]=b->memory[v+q]=uint8_t(flags+j+q);a->memory[v]=b->memory[v]=0;a->memory[v+1]=b->memory[v+1]=0;a->memory[v+2]=b->memory[v+2]=0xcf;a->memory[v+3]=b->memory[v+3]=0;a->memory[v+4]=b->memory[v+4]=uint8_t(j%3);a->memory[v+8]=b->memory[v+8]=0;a->memory[v+9]=b->memory[v+9]=uint8_t((flags+j)%3?j+1:0);}for(unsigned j=0;j<16;++j)a->memory[0xcbed-j]=b->memory[0xcbed-j]=uint8_t(flags+j);uint8_t note=uint8_t(flags & 127); a->reg8(13)=int8_t(note); for(unsigned j=0;j<8;++j){auto v=0xd030+j*18; a->memory[v+5]=b->memory[v+5]=uint8_t((j+kind)%2 ? note : note+1); a->memory[v+17]=b->memory[v+17]=0;} a->memory[0xcf58]=b->memory[0xcf58]=uint8_t(flags); a->memory[0xcc01]=b->memory[0xcc01]=uint8_t(kind); a->reg8(5)=int8_t(flags&127);a->memory[0xbfaa]=b->memory[0xbfaa]=0;a->memory[0xbfab]=b->memory[0xbfab]=0;a->memory[0xbfac]=b->memory[0xbfac]=0xcf;a->memory[0xbfad]=b->memory[0xbfad]=0;a->memory[0xcf81]=b->memory[0xcf81]=uint8_t(kind);a->memory[0xd02c]=b->memory[0xd02c]=0; a->memory[0xcad3]=b->memory[0xcad3]=note;a->memory[0xcad4]=b->memory[0xcad4]=uint8_t(flags&127);a->memory[0xcad5]=b->memory[0xcad5]=uint8_t(flags&15);a->memory[0xbb16]=b->memory[0xbb16]=uint8_t(flags&15);a->memory[0xcd26]=b->memory[0xcd26]=uint8_t(flags);a->memory[0xcd39]=b->memory[0xcd39]=uint8_t(flags%4);a->memory[0xcbfe]=b->memory[0xcbfe]=note;a->reg32(4)=0xcad3;bool started=modulation.start(kind?kstation::MidiExpressionEventTask::Kind::ChannelPressure:kstation::MidiExpressionEventTask::Kind::PitchBend,0xcad3);
  check(started,"DSP start rejected");a->pc=a->makepc(entry);++cases;
  while(modulation.active()){
   x.count=y.count=0;const auto cost=modulation.nextClocks(*b,irq,dsp);check(cost,"DSP next phase empty");
   if(phases%71==0){std::array<unsigned char,sizeof(modulation)>before{};std::memcpy(before.data(),&modulation,sizeof(modulation));auto cycle=b->cycles;auto oldRead=b->lastRead,oldWrite=b->lastWrite;for(unsigned budget=0;budget<cost;++budget){auto r=modulation.advanceBefore(*b,irq,dsp,budget);check(!r.clocks&&cycle==b->cycles&&oldRead==b->lastRead&&oldWrite==b->lastWrite&&y.count==0&&!std::memcmp(before.data(),&modulation,sizeof(modulation)),"DSP short budget mutated");++shorts;}}
   auto r=modulation.advanceBefore(*b,irq,dsp,cost);check(r.clocks==cost,"DSP phase rejected");a->step();if(a->cycles!=b->cycles){std::fprintf(stderr,"kind%u phase%llu pc%x clock%llu/%llu cost%u\n",kind,phases,a->getPC(),a->cycles,b->cycles,cost);throw std::runtime_error("DSP domain clocks differ");}
   if(x.count!=y.count){std::fprintf(stderr,"event count kind%u case%llu phase%llu pc%x %u/%u clocks%llu/%llu\n",kind,cases,phases,a->getPC(),x.count,y.count,a->cycles,b->cycles);for(unsigned i=0;i<x.count;++i)std::fprintf(stderr,"orig %x %x at%llu\n",x.events[i].address,x.events[i].value,x.events[i].clocks);for(unsigned i=0;i<y.count;++i)std::fprintf(stderr,"native %x %x at%llu\n",y.events[i].address,y.events[i].value,y.events[i].clocks);throw std::runtime_error("DSP domain event count");}for(unsigned i=0;i<x.count;++i){auto p=x.events[i],q=y.events[i];if(!(p.clocks==q.clocks&&p.address==q.address&&p.value==q.value&&p.write==q.write)){std::fprintf(stderr,"event PC%x %llx/%llx addr%x/%x val%x/%x write%d/%d\n",a->getPC(),p.clocks,q.clocks,p.address,q.address,p.value,q.value,p.write,q.write);throw std::runtime_error("DSP domain event differs");}}
   ++phases;io+=y.count;check(phases<20000000,"DSP domain stalled");
  }
  check(a->getPC()==0x50000,"rate return differs");check(a->memory[0xc598]==b->memory[0xc598]&&a->memory[0xc599]==b->memory[0xc599]&&x.pins==y.pins&&x.ports==y.ports,"DSP domain final resources");

 }
};
}
void*operator new(std::size_t n){if(watch)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(int argc,char**argv){try{if(argc<2)return 77;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};check(rom.size()==49728,"Private ROM size");Fixture f(rom);watch=true;
 for(unsigned k=0;k<2;++k)for(unsigned flags=0;flags<256;++flags)f.run(k,flags,flags%4,flags&1,flags&2,false);
 for(unsigned k=0;k<2;++k)f.run(k,0xff,0,false,false,true);
 watch=false;check(!allocations,"DSP domain processing allocated");f.x.enabled=f.y.enabled=false;unsigned guards=0;for(auto range:{std::pair{0x2d38u,0x2d4cu},std::pair{0x2d5cu,0x2d7eu},std::pair{0x2df0u,0x2e34u},std::pair{0x72ceu,0x72e2u},std::pair{0x80ceu,0x80f4u},std::pair{0x8140u,0x816cu}})for(auto a=range.first;a<range.second;++a)for(auto alias:{0u,0x400000u}){auto addr=a+alias;auto old=f.b->readResource(addr);bool rejected=false;try{f.b->writeResource(addr,uint8_t(old^1));}catch(const std::logic_error&){rejected=true;}check(rejected&&f.b->readResource(addr)==old,"expression immutable guard");++guards;}check(guards==448,"expression guard count");std::printf("{\"passed\":true,\"graphs\":%llu,\"original_phases\":%llu,\"ordered_io\":%llu,\"short_budget_checks\":%llu,\"processing_allocations\":%llu}\n",f.cases,f.phases,f.io,f.shorts,allocations);return 0;
 }catch(const std::exception&e){watch=false;std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
