#include "kstationHdiWait.h"
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <vector>

namespace {
bool watch=false;uint64_t allocations=0;
void check(bool ok,const char* what){if(!ok){std::cerr<<what<<'\n';std::exit(1);}}
struct Access {uint64_t cycle;uint32_t pc,address;uint8_t value;};
struct Port final:H8SDevice {
 uint8_t value=0;std::array<Access,8> log{};unsigned count=0;
 uint8_t read(uint32_t address)override{check(count<log.size(),"Port log overflow");log[count++]={state->cycles,uint32_t(state->getPC()),address,value};return value;}
 void write(uint32_t,uint8_t)override{check(false,"Unexpected HDI write");}
};
void equal(const kstation::Stage1Cpu&a,const kstation::Stage1Cpu&b,const Port&x,const Port&y){
 check(a.getPC()==b.getPC()&&a.cycles==b.cycles&&a.ccr==b.ccr&&a.exr==b.exr&&a.pending_irqs==b.pending_irqs&&a.pending_irqs_high==b.pending_irqs_high&&a.readHistory()==b.readHistory()&&a.writeHistory()==b.writeHistory(),"Native HDI boundary/frame timing mismatch");
 for(unsigned n=0;n<8;++n)check(a.regs[n].er==b.regs[n].er,"Native HDI register mismatch");
 check(x.count==y.count,"Native HDI read count mismatch");
 for(unsigned n=0;n<x.count;++n)check(x.log[n].cycle==y.log[n].cycle&&x.log[n].pc==y.log[n].pc&&x.log[n].address==y.log[n].address&&x.log[n].value==y.log[n].value,"Native HDI live IO mismatch");
}
}
void*operator new(size_t n){if(watch)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}

int main(int argc,char**argv){
 if(argc!=2){std::cout<<"SKIP: decoded private K-Station OS required\n";return 77;}
 std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};check(rom.size()==49728,"Wrong ROM");
 auto ref=std::make_unique<kstation::Stage1Cpu>(),native=std::make_unique<kstation::Stage1Cpu>();
 ref->loadmem(rom.data(),unsigned(rom.size()),0);native->loadmem(rom.data(),unsigned(rom.size()),0);
 Port a,b;ref->memmap(&a,0x600000,16);native->memmap(&b,0x600000,16);
 uint64_t boundaries=0,guards=0;uint32_t random=0x516ea29;
 const std::array<unsigned,10> offsets{{0,8,10,12,14,22,24,26,28,34}};
 const std::array<uint16_t,8> counters{{0,1,2,0x7fff,0x8000,0x8001,0xfffe,0xffff}};
 for(const auto site:kstation::NativeHdiWait::Sites)for(unsigned offset:offsets)for(unsigned flags=0;flags<256;++flags)for(auto counter:counters){
  random=random*1664525+1013904223;
  for(unsigned n=0;n<8;++n)ref->regs[n].er=native->regs[n].er=int32_t(random^(n*0x67890123u));
  ref->regs[5].er=native->regs[5].er=0x600000;ref->reg16(8)=native->reg16(8)=int16_t(counter);
  ref->reg8(8)=native->reg8(8)=int8_t(random>>24);ref->ccr=native->ccr=uint8_t(flags);ref->exr=native->exr=uint8_t(random);
  ref->pc=ref->makepc(int(site.begin+offset));native->pc=native->makepc(int(site.begin+offset));
  ref->cycles=native->cycles=uint64_t(random)*3;ref->pending_irqs=native->pending_irqs=0;
  ref->histories(int(random),int(random>>1));native->histories(int(random),int(random>>1));
  a.value=b.value=uint8_t(random>>16);a.count=b.count=0;
  watch=true;ref->step();check(kstation::NativeHdiWait::step(*native),"Expected native phase");watch=false;
  equal(*ref,*native,a,b);++boundaries;
 }
 // Every status byte on both read phases; real mapped callbacks, not a tape.
 for(const auto site:kstation::NativeHdiWait::Sites)for(unsigned offset:{0u,14u})for(unsigned value=0;value<256;++value){
  ref->pc=ref->makepc(int(site.begin+offset));native->pc=native->makepc(int(site.begin+offset));ref->cycles=native->cycles=0;
  ref->ccr=native->ccr=0xff;ref->histories(-1,-1);native->histories(-1,-1);a.value=b.value=uint8_t(value);a.count=b.count=0;
  watch=true;ref->step();check(kstation::NativeHdiWait::step(*native),"Status phase missing");watch=false;equal(*ref,*native,a,b);++boundaries;
 }
 for(unsigned flags:{0u,128u})for(unsigned vector:{28u,38u,52u,53u,54u,60u}){
  native->pc=native->makepc(0x178a);native->ccr=uint8_t(flags);native->pending_irqs=uint64_t(1)<<vector;
  const auto cycles=native->cycles;const auto pc=native->getPC();a.count=b.count=0;
  check(!kstation::NativeHdiWait::step(*native)&&native->cycles==cycles&&native->getPC()==pc,"IRQ admission must stay original without mutation");++guards;
 }
 uint64_t graphs=0,interrupts=0;
 for(const auto site:kstation::NativeHdiWait::Sites)for(unsigned readyAfter:{0u,1u,2u,7u,31u})for(unsigned inject:{0u,1u}){
  for(unsigned n=0;n<8;++n)ref->regs[n].er=native->regs[n].er=0;
  ref->regs[5].er=native->regs[5].er=0x600000;ref->setSP(0xffff10);native->setSP(0xffff10);
  ref->pending_irqs=native->pending_irqs=0;ref->ccr=native->ccr=0;
  ref->pc=ref->makepc(int(site.begin));native->pc=native->makepc(int(site.begin));ref->histories(-1,-1);native->histories(-1,-1);ref->cycles=native->cycles=0;
  unsigned reads=0,steps=0;bool injected=false;
  while(uint32_t(ref->getPC())!=site.begin+36){
   check(++steps<1000,"Wait graph did not complete");a.count=b.count=0;a.value=b.value=reads>=readyAfter?site.mask:0;
   if(inject&&!injected&&steps==4){ref->interrupt(2);native->interrupt(2);injected=true;++interrupts;}
   watch=true;ref->step();if(!kstation::NativeHdiWait::step(*native))native->step();watch=false;
   equal(*ref,*native,a,b);reads+=a.count;++boundaries;
  }
  check(std::memcmp(ref->memory+0xfffef0,native->memory+0xfffef0,32)==0,"IRQ stack bytes differ");++graphs;
 }
 native->pending_irqs=0;native->pending_irqs_high=1;native->pc=native->makepc(0x178a);check(!kstation::NativeHdiWait::step(*native),"Pending high guard");++guards;native->pending_irqs_high=0;
 for(unsigned pc:{0x100u,0x1690u,0x16a0u,0x17a0u}){native->pc=native->makepc(int(pc));const auto c=native->cycles;check(!kstation::NativeHdiWait::step(*native)&&native->cycles==c&&native->getPC()==int(pc),"Outside/gap must not mutate");++guards;}
 native->pc=native->makepc(0x178a);native->regs[5].er=0;const auto beforeGuard=native->cycles;bool rejected=false;
 try{kstation::NativeHdiWait::step(*native);}catch(const std::logic_error&){rejected=true;}
 check(rejected&&native->cycles==beforeGuard&&native->getPC()==0x178a,"Invalid port must reject before mutation");++guards;native->regs[5].er=0x600000;
 native->pending_irqs=0;native->interruptMode2=true;check(!kstation::NativeHdiWait::step(*native),"Mode2 must not enter K task");++guards;native->interruptMode2=false;
 check(allocations==0,"Processing allocated");
 std::cout<<"PASS HDI boundaries="<<boundaries<<" complete-graphs="<<graphs<<" injected-IRQs="<<interrupts<<" guards="<<guards<<" allocations="<<allocations<<'\n';return 0;
}
