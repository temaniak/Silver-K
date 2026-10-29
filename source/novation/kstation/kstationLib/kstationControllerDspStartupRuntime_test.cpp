#include "kstationControllerDspStartupRuntime.h"
#include "h8s/h8s.hpp"
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
 S&state;std::array<Event,32>events{};unsigned count=0,group=0,reads=0,delay=0;bool enabled=false,fault=false,recovered=false;uint8_t forceStatus=0;std::array<uint8_t,16>ports{};uint8_t pins=0x82;
 explicit Tape(S&s):state(s){}
 uint8_t readValue(uint32_t a){uint8_t v=state.memory[a];if(a==0x600004){++reads;v=fault&&!recovered?forceStatus:reads<=delay?0:uint8_t(group==1?16:7);}else if(a>=0x600000&&a<0x600010)v=ports[a-0x600000];else if(a==0xffffd2)v=pins;else if(a==0xfffff2)v=0;
 if(enabled){check(count<events.size(),"DSP tape capacity");events[count++]={state.cycles,a,v,false};}return v;}
 void writeValue(uint32_t a,uint8_t v){if(enabled){check(count<events.size(),"DSP tape capacity");events[count++]={state.cycles,a,v,true};}if(a>=0x600000&&a<0x600010){ports[a-0x600000]=v;if(a==0x600000&&v==8){group=1;reads=0;}if(a==0x60000e){group=2;reads=0;}}else if(a==0xffffd2){pins=v;recovered=true;}else state.memory[a]=v;}
};
struct OriginalBus:public H8SDevice {Tape<Original>&t;explicit OriginalBus(Tape<Original>&v):t(v){}uint8_t read(uint32_t a)override{return t.readValue(a);}void write(uint32_t a,uint8_t v)override{t.writeValue(a,v);}};
struct NativeBus:public kstation::ControllerResourceBus {Tape<kstation::ControllerDeviceState>&t;explicit NativeBus(Tape<kstation::ControllerDeviceState>&v):t(v){}bool mapped(uint32_t a)const override{return (a>=0xc598&&a<0xc59a)||(a>=0x600000&&a<0x600010)||a>=0xffffa0;}uint8_t read(uint32_t a)override{return t.readValue(a);}void write(uint32_t a,uint8_t v)override{t.writeValue(a,v);}};
struct Fixture {
 std::unique_ptr<Original>a=std::make_unique<Original>();std::unique_ptr<kstation::ControllerDeviceState>b=std::make_unique<kstation::ControllerDeviceState>();Tape<Original>x{*a};Tape<kstation::ControllerDeviceState>y{*b};OriginalBus ob{x};NativeBus nb{y};kstation::ControllerInterruptRuntime irq;kstation::ControllerDspTransferRuntime dsp;kstation::ControllerDspStartupRuntime startup;uint64_t cases=0,phases=0,io=0,shorts=0;
 explicit Fixture(const std::vector<uint8_t>&rom){a->loadmem(rom.data(),unsigned(rom.size()),0);b->loadMemory(rom.data(),rom.size(),0);b->loadMemory(rom.data(),rom.size(),0x400000);a->memmap(&ob,0xc598,2);a->memmap(&ob,0x600000,16);a->memmap(&ob,0xffffa0,0x60);b->attachBus(&nb);check(irq.bindVerifiedFirmware(*b,rom.data(),rom.size()),"DSP IRQ binding");check(dsp.bindVerifiedFirmware(*b,rom.data(),rom.size()),"DSP resource binding");check(startup.bindVerifiedFirmware(*b,rom.data(),rom.size()),"Bootstrap resource binding");}
 void run(unsigned kind,unsigned flags,unsigned delay,bool cacheMatch,bool pending,bool fault){
  startup.reset();dsp.reset();a->cycles=b->cycles=100;for(auto&v:a->regs)v.er=0;a->reg32(5)=0x600000;a->setSP(0xffff00);a->memory[0xffff00]=0;a->memory[0xffff01]=5;a->memory[0xffff02]=0;a->memory[0xffff03]=0;
  const auto address=uint16_t((cases*773)^0x8040);const auto word=uint32_t(cases*1447)^0xabcdef;auto cache=uint16_t(cacheMatch?address:address+1);a->memory[0xc598]=b->memory[0xc598]=uint8_t(cache>>8);a->memory[0xc599]=b->memory[0xc599]=uint8_t(cache);
  a->reg16(4)=int16_t(address);a->reg16(9)=int16_t(word>>8);a->reg8(1)=int8_t(word);a->ccr=uint8_t(flags|0x80);a->exr=0;a->pending_irqs=b->pendingIrqs=pending?(uint64_t(1)<<36):0;a->pending_irqs_high=b->pendingIrqsHigh=0;a->interruptMode2=b->interruptMode2=false;a->histories(-1,-1);b->lastRead=b->lastWrite=-1;irq.setMask(kstation::ControllerInterruptRuntime::Mask::Disabled);
  x.enabled=y.enabled=true;x.count=y.count=0;x.group=y.group=0;x.reads=y.reads=0;x.delay=y.delay=delay;x.fault=y.fault=fault;x.recovered=y.recovered=false;x.ports.fill(0);y.ports.fill(0);x.ports[10]=y.ports[10]=0x12;x.ports[12]=y.ports[12]=0x34;x.ports[14]=y.ports[14]=0x56;x.pins=y.pins=0x82;
  unsigned entry=0;bool started=false;uint32_t source=0x18000;uint16_t words=uint16_t(kind==4?17:kind==2?uint16_t(flags%3==0?1:flags%3==1?17:255):0);bool armed=false;
  if(kind==0||kind==3){entry=0x14ee;started=startup.start();}else if(kind==1){entry=0x2ae;started=startup.startReset();}else{entry=0x155e;started=startup.startStream(source,words);a->reg32(4)=int32_t(source);a->reg16(11)=int16_t(words);}
  x.forceStatus=y.forceStatus=0;if(kind!=4)x.fault=y.fault=false;
  check(started,"DSP bootstrap start rejected");a->pc=a->makepc(entry);++cases;
  while(startup.active()){
   if(kind==3&&a->getPC()==0x1524&&!armed){armed=true;x.fault=y.fault=true;x.recovered=y.recovered=false;x.forceStatus=y.forceStatus=16;}
   if(kind==2&&(a->getPC()==0x1582||a->getPC()==0x158c||a->getPC()==0x1596)){auto address=uint32_t(a->reg32(4))&0xffffff;check((startup.streamByteReference()&0xffffff)==address,"Bootstrap data cursor differs");if(phases%31==0)a->memory[address]=b->memory[address]=uint8_t(phases);}
   x.count=y.count=0;const auto cost=startup.nextClocks(*b,irq,dsp);check(cost,"DSP next phase empty");
   if(phases%71==0){std::array<unsigned char,sizeof(startup)>before{};std::memcpy(before.data(),&startup,sizeof(startup));auto cycle=b->cycles;auto oldRead=b->lastRead,oldWrite=b->lastWrite;for(unsigned budget=0;budget<cost;++budget){auto r=startup.advanceBefore(*b,irq,dsp,budget);check(!r.clocks&&cycle==b->cycles&&oldRead==b->lastRead&&oldWrite==b->lastWrite&&y.count==0&&!std::memcmp(before.data(),&startup,sizeof(startup)),"DSP short budget mutated");++shorts;}}
   auto r=startup.advanceBefore(*b,irq,dsp,cost);check(r.clocks==cost,"DSP phase rejected");a->step();if(a->cycles!=b->cycles){std::fprintf(stderr,"kind%u phase%llu pc%x clock%llu/%llu cost%u\n",kind,phases,a->getPC(),a->cycles,b->cycles,cost);throw std::runtime_error("DSP domain clocks differ");}
   if(x.count!=y.count){std::fprintf(stderr,"event count kind%u case%llu phase%llu pc%x %u/%u clocks%llu/%llu\n",kind,cases,phases,a->getPC(),x.count,y.count,a->cycles,b->cycles);for(unsigned i=0;i<x.count;++i)std::fprintf(stderr,"orig %x %x at%llu\n",x.events[i].address,x.events[i].value,x.events[i].clocks);for(unsigned i=0;i<y.count;++i)std::fprintf(stderr,"native %x %x at%llu\n",y.events[i].address,y.events[i].value,y.events[i].clocks);throw std::runtime_error("DSP domain event count");}for(unsigned i=0;i<x.count;++i){auto p=x.events[i],q=y.events[i];if(!(p.clocks==q.clocks&&p.address==q.address&&p.value==q.value&&p.write==q.write)){std::fprintf(stderr,"event PC%x %llx/%llx addr%x/%x val%x/%x write%d/%d\n",a->getPC(),p.clocks,q.clocks,p.address,q.address,p.value,q.value,p.write,q.write);throw std::runtime_error("DSP domain event differs");}}
   ++phases;io+=y.count;check(phases<20000000,"DSP domain stalled");
  }
  check(a->getPC()==0x50000,"DSP return differs");check(a->memory[0xc598]==b->memory[0xc598]&&a->memory[0xc599]==b->memory[0xc599]&&x.pins==y.pins&&x.ports==y.ports,"DSP domain final resources");

 }
};
}
void*operator new(std::size_t n){if(watch)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(int argc,char**argv){try{if(argc<2)return 77;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};check(rom.size()==49728,"Private ROM size");Fixture f(rom);watch=true;
 for(unsigned k:{0u,1u})for(unsigned flags:{0u,0x2fu,0x80u,0xffu})f.run(k,flags,flags%4,false,flags&2,false);
 for(unsigned flags=0;flags<256;++flags)f.run(2,flags,flags%4,false,flags&2,false);
 f.run(3,0xff,0,false,true,false);f.run(4,0xff,0,false,true,true);
 watch=false;check(!allocations,"DSP domain processing allocated");std::printf("{\"passed\":true,\"graphs\":%llu,\"original_phases\":%llu,\"ordered_io\":%llu,\"short_budget_checks\":%llu,\"processing_allocations\":%llu}\n",f.cases,f.phases,f.io,f.shorts,allocations);return 0;
 }catch(const std::exception&e){watch=false;std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
