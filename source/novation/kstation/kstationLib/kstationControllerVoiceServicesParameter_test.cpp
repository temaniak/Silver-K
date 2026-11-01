#include "kstationControllerVoiceServices.h"
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
template<class S>struct Tape {S&state;std::array<Event,64>events{};unsigned count=0,group=0;bool enabled=false;explicit Tape(S&s):state(s){}uint8_t readValue(uint32_t a){auto v=a==0x600004?uint8_t(group==1?16:7):state.memory[a];if(enabled)events[count++]={state.cycles,a,v,false};return v;}void writeValue(uint32_t a,uint8_t v){if(enabled)events[count++]={state.cycles,a,v,true};state.memory[a]=v;if(a==0x600000&&v==8)group=1;if(a==0x60000e)group=2;}};
struct OriginalBus:public H8SDevice {Tape<Original>&t;explicit OriginalBus(Tape<Original>&v):t(v){}uint8_t read(uint32_t a)override{return t.readValue(a);}void write(uint32_t a,uint8_t v)override{t.writeValue(a,v);}};
struct NativeBus:public kstation::ControllerResourceBus {Tape<kstation::ControllerDeviceState>&t;explicit NativeBus(Tape<kstation::ControllerDeviceState>&v):t(v){}bool mapped(uint32_t a)const override{return (a>=0xbf98&&a<0xc16c)||(a>=0xba5c&&a<0xbb30)||(a>=0xd0f0&&a<0xd0f1)||(a>=0xcf00&&a<0xd000)||(a>=0xc598&&a<0xc59a)||(a>=0x600000&&a<0x600010)||(a>=0xd022&&a<0xd080)||a==0xfffff2;}uint8_t read(uint32_t a)override{return t.readValue(a);}void write(uint32_t a,uint8_t v)override{t.writeValue(a,v);}};
struct Fixture {
 std::unique_ptr<Original>a=std::make_unique<Original>();std::unique_ptr<kstation::ControllerDeviceState>b=std::make_unique<kstation::ControllerDeviceState>();Tape<Original>x{*a};Tape<kstation::ControllerDeviceState>y{*b};OriginalBus ob{x};NativeBus nb{y};kstation::ControllerInterruptRuntime irq;kstation::ControllerDspTransferRuntime dsp;kstation::ControllerVoiceServices services;uint64_t cases=0,phases=0,io=0,shorts=0,children=0,rebuilds=0,addresses=0;
 explicit Fixture(const std::vector<uint8_t>&rom){a->loadmem(rom.data(),unsigned(rom.size()),0);b->loadMemory(rom.data(),rom.size(),0);b->loadMemory(rom.data(),rom.size(),0x400000);a->memmap(&ob,0xbf98,0x1d4);a->memmap(&ob,0xba5c,0xd4);a->memmap(&ob,0xd0f0,1);a->memmap(&ob,0xcf00,0x100);a->memmap(&ob,0xc598,2);a->memmap(&ob,0x600000,16);a->memmap(&ob,0xd022,0x5e);a->memmap(&ob,0xfffff2,1);b->attachBus(&nb);check(irq.bindVerifiedFirmware(*b,rom.data(),rom.size()),"IRQ binding");check(dsp.bindVerifiedFirmware(*b,rom.data(),rom.size()),"dsp binding");check(services.bindAll(*b,rom.data(),rom.size()),"voice bindings");auto saved=b->memory[0xc1cc];b->write8(0xc1cc,uint8_t(saved^0x5a));check(b->memory[0xc1cc]==uint8_t(saved^0x5a),"voice bindings block live ADC RAM");b->write8(0xc1cc,saved);}
 void run(unsigned kind,unsigned flags,unsigned count,bool match,bool pending,bool inhibited){dsp.reset();x.enabled=y.enabled=true;x.group=y.group=0;a->cycles=b->cycles=100;for(unsigned i=0;i<256;++i)a->memory[0xcf00+i]=b->memory[0xcf00+i]=uint8_t(i*53+flags*7);for(auto&v:a->regs)v.er=0;a->setSP(0xffff00);a->memory[0xffff00]=0;a->memory[0xffff01]=5;a->memory[0xffff02]=0;a->memory[0xffff03]=0;for(unsigned i=0;i<0x5e;++i)a->memory[0xd022+i]=b->memory[0xd022+i]=uint8_t(i*17+flags);a->memory[0xd022]=b->memory[0xd022]=uint8_t(count+1);a->memory[0xd028]=b->memory[0xd028]=uint8_t(flags);a->memory[0xbfc8]=b->memory[0xbfc8]=inhibited;for(unsigned i=0;i<count+1;++i){auto v=0xd030+i*18;a->memory[v]=b->memory[v]=0;a->memory[v+1]=b->memory[v+1]=0;a->memory[v+2]=b->memory[v+2]=0xcf;a->memory[v+3]=b->memory[v+3]=0;a->memory[v+4]=b->memory[v+4]=uint8_t(match?flags:flags+1);a->memory[v+8]=b->memory[v+8]=uint8_t(i&1);a->memory[v+9]=b->memory[v+9]=uint8_t(1+i);}a->ccr=uint8_t(flags|0x80);a->exr=0;a->pending_irqs=b->pendingIrqs=pending?(uint64_t(1)<<36):0;a->pending_irqs_high=b->pendingIrqsHigh=0;a->interruptMode2=b->interruptMode2=false;a->histories(-1,-1);b->lastRead=b->lastWrite=-1;irq.setMask(kstation::ControllerInterruptRuntime::Mask::Disabled);a->pc=a->makepc(kind==0?0x785c:kind==1?0x78c2:0x7930);check(services.startParameterRefresh(),"refresh start");++cases;
 while(services.refreshActive()){x.count=y.count=0;auto route=services.routeParameterRefresh(*b,dsp);check(route==kstation::ControllerVoiceServices::Route::Ready,"voice route");auto cost=services.nextParameterRefresh(*b,irq,dsp);if(!cost)continue;if(phases%31==0)for(unsigned budget=0;budget<cost;++budget){auto t=b->cycles;auto r=services.advanceParameterRefresh(*b,irq,dsp,budget);check(!r.clocks&&t==b->cycles&&!y.count,"short budget");++shorts;}auto r=services.advanceParameterRefresh(*b,irq,dsp,cost);check(r.clocks==cost,"refresh phase");a->step();if(a->cycles!=b->cycles){std::fprintf(stderr,"kind%u case%llu phase%llu pc%x clock%llu/%llu\n",kind,cases,phases,a->getPC(),a->cycles,b->cycles);throw std::runtime_error("clocks differ");}check(x.count==y.count,"event count");for(unsigned i=0;i<x.count;++i){auto p=x.events[i],q=y.events[i];if(!(p.clocks==q.clocks&&p.address==q.address&&p.value==q.value&&p.write==q.write)){std::fprintf(stderr,"case%llu phase%llu pc%x event %x/%x t%llu/%llu v%x/%x w%d/%d\n",cases,phases,a->getPC(),p.address,q.address,p.clocks,q.clocks,p.value,q.value,p.write,q.write);throw std::runtime_error("event differs");}}++phases;io+=y.count;check(phases<20000000,"stalled");}
 check(a->getPC()==0x50000,"return differs");for(unsigned i=0;i<0x5e;++i)check(a->memory[0xd022+i]==b->memory[0xd022+i],"resource differs");}

};
}
void*operator new(std::size_t n){if(watch)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(int argc,char**argv){try{if(argc<2)return 77;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};check(rom.size()==49728,"Private ROM size");Fixture f(rom);watch=true;
 for(unsigned k=2;k<3;++k)for(unsigned flags=0;flags<256;++flags)f.run(k,flags,flags%4,flags&1,flags&2,false);
 
 watch=false;check(!allocations,"DSP domain processing allocated");std::printf("{\"passed\":true,\"graphs\":%llu,\"original_phases\":%llu,\"ordered_io\":%llu,\"short_budget_checks\":%llu,\"children\":%llu,\"processing_allocations\":%llu}\n",f.cases,f.phases,f.io,f.shorts,f.children,allocations);return 0;
 }catch(const std::exception&e){watch=false;std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
