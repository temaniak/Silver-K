#include "kstationNativeSerialRx.h"
#include "kstationNativeIrq.h"
#ifndef KSTATION_RX_CLOCK_ORIGINAL_SIDECAR
#include "kstationNativeClock.h"
#endif
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <vector>
#include <cstring>
#include <new>
#if defined(_MSC_VER)
#include <malloc.h>
#endif
namespace {
bool watch=false;uint64_t allocations=0;
void* allocate(size_t n){if(watch)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
uint32_t seed=0x4b535258;uint32_t randomValue(){seed^=seed<<13;seed^=seed>>17;return seed^=seed<<5;}
class Original final:public H8SEmulator{public:int readHistory()const{return lastread;}int writeHistory()const{return lastwrite;}void histories(int r,int w){lastread=r;lastwrite=w;}int irqDepth()const{return indent;}void irqDepth(int n){indent=n;}};
struct Region{uint32_t at,size;};
constexpr Region Regions[]={{0,16},{0xd4,4},{0xfffd00,0x300},{0xfff0,32},{0x1ffff0,32},{0x3ffff0,48}};
constexpr uint32_t Stacks[]={0xffff10,0xfffd12,0x200001,0x3fffff,0x400001,0x1000001,0x100ffff,0xfffe77};
struct Access{uint64_t time,pending,high;uint32_t pc,sp,r0,r1,r2,at;int depth;uint8_t ccr,exr,value;bool write;};
template<class C>class Bus final:public H8SDevice{public:explicit Bus(C&c):cpu(c){}C&cpu;std::array<Access,64>events{};unsigned count=0;
 void record(uint32_t a,uint8_t v,bool w){check(count<events.size(),"RX IO overflow");events[count++]={cpu.cycles,cpu.pending_irqs,cpu.pending_irqs_high,uint32_t(cpu.getPC()),cpu.getSP(),uint32_t(cpu.reg32(0)),uint32_t(cpu.reg32(1)),uint32_t(cpu.reg32(2)),a,cpu.irqDepth(),cpu.ccr,cpu.exr,v,w};}
 uint8_t read(uint32_t a)override{const auto v=cpu.memory[a];record(a,v,false);return v;}void write(uint32_t a,uint8_t v)override{record(a,v,true);cpu.memory[a]=v;}
};
struct Metrics{uint64_t handlers=0,rxPhases=0,clockPhases=0,originalClockSidecar=0,io=0,entries=0,returns=0,clockCalls=0,mutations=0,nested=0,budgets=0,guards=0,pureIntervals=0,pureRejected=0,overflowCases=0;};
class Fixture{public:
 std::unique_ptr<Original>a=std::make_unique<Original>();std::unique_ptr<kstation::Stage1Cpu>b=std::make_unique<kstation::Stage1Cpu>();Bus<Original>x{*a};Bus<kstation::Stage1Cpu>y{*b};kstation::NativeSerialRx native;
#ifndef KSTATION_RX_CLOCK_ORIGINAL_SIDECAR
 kstation::NativeClock clock;
#endif
 const std::vector<uint8_t>&rom;Metrics m;uint32_t dataAt=0;
 explicit Fixture(const std::vector<uint8_t>&r):rom(r){for(auto q:Regions){a->memmap(&x,q.at,q.size);b->memmap(&y,q.at,q.size);}a->loadmem(r.data(),unsigned(r.size()),0);b->loadmem(r.data(),unsigned(r.size()),0);}
 void byte(uint32_t at,unsigned v){a->memory[at]=b->memory[at]=uint8_t(v);}
 void word(uint32_t at,unsigned v){byte(at,v>>8);byte(at+1,v);}
 void prepare(unsigned flags,unsigned data,unsigned producer,unsigned consumer,unsigned status,uint32_t sp,unsigned ue){
  native.reset();
#ifndef KSTATION_RX_CLOCK_ORIGINAL_SIDECAR
  clock.reset();
#endif
  for(auto q:Regions)for(unsigned n=0;n<q.size;++n)a->memory[q.at+n]=b->memory[q.at+n]=uint8_t(n*73+19);
  for(unsigned n=0;n<4;++n)byte(0xd4+n,rom[0xd4+n]);
  for(unsigned n=0;n<8;++n)a->regs[n].er=b->regs[n].er=int32_t(randomValue());
  a->setSP(sp);b->setSP(sp);a->pc=a->makepc(0x3ae8);b->pc=b->makepc(0x3ae8);a->ccr=b->ccr=uint8_t(flags&127);a->exr=b->exr=uint8_t(flags);a->irqDepth(2);b->irqDepth(2);a->cycles=b->cycles=uint64_t(randomValue())*3;a->pending_irqs=b->pending_irqs=uint64_t(1)<<53;a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=false;a->histories(0x200000,0xfffe72);b->histories(0x200000,0xfffe72);
  byte(0xfffff2,ue);byte(0xffffb4,status);byte(0xffffb5,data);word(0xfffd2c,producer);word(0xfffd2e,consumer);byte(0xfffe30,0xac);
  // Stable, non-dividing initial F8 tracking path. Shared clock owns wider
  // arithmetic tests; this fixture proves RX child dispatch/resumption.
  byte(0xfffe6e,0);byte(0xfffe72,9);word(0xfffd28,0);word(0xffff86,0);byte(0xffff85,0);
  dataAt=(0xfffd30u+producer)&0xffffff;check(!(dataAt>=0xa64&&dataAt<0xac2),"RX test code alias");a->memmap(&x,int(dataAt),1);b->memmap(&y,int(dataAt),1);byte(dataAt,0x59);x.count=y.count=0;
 }
 void equalFrame(){
  bool ok=a->cycles==b->cycles&&a->getPC()==b->getPC()&&a->ccr==b->ccr&&a->exr==b->exr&&a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->interruptMode2==b->interruptMode2&&a->readHistory()==b->readHistory()&&a->writeHistory()==b->writeHistory()&&a->irqDepth()==b->irqDepth();for(unsigned n=0;n<8;++n)ok&=a->regs[n].er==b->regs[n].er;
  if(!ok){watch=false;std::fprintf(stderr,"RX frame PC %x/%x clocks %llu/%llu CCR %x/%x SP %x/%x R0 %x/%x R1 %x/%x R2 %x/%x\n",a->getPC(),b->getPC(),a->cycles,b->cycles,a->ccr,b->ccr,a->getSP(),b->getSP(),unsigned(a->reg32(0)),unsigned(b->reg32(0)),unsigned(a->reg32(1)),unsigned(b->reg32(1)),unsigned(a->reg32(2)),unsigned(b->reg32(2)));throw std::runtime_error("RX full frame mismatch");}
  check(x.count==y.count,"RX IO count mismatch");for(unsigned n=0;n<x.count;++n){const auto&p=x.events[n];const auto&q=y.events[n];check(p.time==q.time&&p.pending==q.pending&&p.high==q.high&&p.pc==q.pc&&p.sp==q.sp&&p.r0==q.r0&&p.r1==q.r1&&p.r2==q.r2&&p.at==q.at&&p.depth==q.depth&&p.ccr==q.ccr&&p.exr==q.exr&&p.value==q.value&&p.write==q.write,"RX live IO context mismatch");}
 }
 void equalMemory(){for(auto q:Regions)check(std::memcmp(a->memory+q.at,b->memory+q.at,q.size)==0,"RX RAM/canary mismatch");check(a->memory[dataAt]==b->memory[dataAt],"RX ring store mismatch");}
 kstation::NativeSerialRx::Result step(){x.count=y.count=0;auto r=native.step(*b);
  if(r.stepped){++m.rxPhases;m.entries+=r.entered;m.returns+=r.returned;m.clockCalls+=r.clockRequested;}
  else {
#ifdef KSTATION_RX_CLOCK_ORIGINAL_SIDECAR
   check(native.active()&&(b->getPC()<0xa64||b->getPC()>=0xac2),"RX unexpected sidecar scope");b->step();++m.originalClockSidecar;
#else
   check(kstation::NativeClock::candidate(uint32_t(b->getPC()))&&clock.step(*b),"RX shared clock did not consume child");++m.clockPhases;
#endif
  }
  a->step();equalFrame();m.io+=y.count;return r;
 }
 void whole(bool mutate=false){bool done=false;for(unsigned n=0;n<1000&&!done;++n){if(mutate){if(a->getPC()==0xa76){byte(0xffffb5,0x91);++m.mutations;}if(a->getPC()==0xa9a){word(0xfffd2e,randomValue()&255);++m.mutations;}}done=step().returned;}check(done&&!native.active(),"RX whole handler incomplete");equalMemory();++m.handlers;}
};
}
void*operator new(std::size_t n){return allocate(n);}void*operator new[](std::size_t n){return allocate(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
void*operator new(std::size_t n,std::align_val_t alignment){if(watch)++allocations;
#if defined(_MSC_VER)
 if(auto*p=_aligned_malloc(n?n:1,static_cast<std::size_t>(alignment)))return p;
#else
 void*p=nullptr;if(posix_memalign(&p,static_cast<std::size_t>(alignment),n?n:1)==0)return p;
#endif
 throw std::bad_alloc();}
void*operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p,std::align_val_t)noexcept{
#if defined(_MSC_VER)
 _aligned_free(p);
#else
 std::free(p);
#endif
}
void operator delete[](void*p,std::align_val_t a)noexcept{::operator delete(p,a);}void operator delete(void*p,std::size_t,std::align_val_t a)noexcept{::operator delete(p,a);}void operator delete[](void*p,std::size_t,std::align_val_t a)noexcept{::operator delete(p,a);}

int main(int argc,char**argv){try{
 if(argc<2){std::puts("SKIP: private verified K OS required");return 77;}std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};check(rom.size()==49728,"ROM size");check(rom[0xd4]==0&&rom[0xd5]==0&&rom[0xd6]==0xa&&rom[0xd7]==0x64,"RX vector");Fixture f(rom);watch=true;
 for(unsigned flags=0;flags<256;++flags)for(unsigned branch=0;branch<3;++branch)for(unsigned ue:{0u,8u})for(unsigned sp=0;sp<4;++sp){f.prepare(flags,branch==2?0xf8:0x91,255,branch==1?0:17,randomValue(),Stacks[sp],ue);f.whole();if(branch==1)++f.m.overflowCases;}
 for(unsigned data=0;data<256;++data)for(unsigned flags:{0u,0x2fu,0x80u,0xffu})for(unsigned ue:{0u,8u}){f.prepare(flags,data,randomValue()&255,randomValue()&255,randomValue(),Stacks[data%8],ue);f.whole();}
 constexpr unsigned Indices[]={0,1,127,128,254,255,256,0x7ffe,0x7fff,0x8000,0xfffe,0xffff};
 for(unsigned p:Indices)for(unsigned c:Indices)for(unsigned flags:{0u,0x2fu,0x80u,0xffu})for(auto sp:Stacks){f.prepare(flags,0x91,p,c,randomValue(),sp,8);f.whole();}
 for(unsigned status=0;status<256;++status){f.prepare(status,0x80,255,0,status,Stacks[0],0);f.whole(true);}
 for(auto sp:Stacks){f.prepare(0x2f,0x91,255,1,0xff,sp,0);f.step();f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<28;f.whole();}
 for(unsigned flags:{0u,0x2fu,0x7fu}){f.prepare(flags,0x91,255,1,0xff,Stacks[0],8);f.step();f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<38;bool ret=false;for(unsigned n=0;n<12&&!ret;++n){f.x.count=f.y.count=0;auto r=kstation::NativeIrq::step(*f.b);check(r.stepped,"Nested timer rejected");f.a->step();f.equalFrame();ret=r.returned;}check(ret,"Nested timer return");f.whole();++f.m.nested;}
 for(unsigned flags:{0u,0x2fu,0x7fu}){f.prepare(flags,0x91,0,17,0xff,Stacks[0],8);f.step();f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<53;f.step();check(f.native.depth()==2,"Nested RX owner");unsigned returned=0;for(unsigned n=0;n<100&&returned<2;++n)if(f.step().returned)++returned;check(returned==2&&!f.native.active(),"Nested RX return");f.equalMemory();++f.m.nested;}
 for(unsigned budget:{0u,1u,2u,3u,4u,5u,7u,8u,16u,31u,64u,127u,128u,255u})for(unsigned branch=0;branch<3;++branch){f.prepare(0,branch==2?0xf8:0x91,255,branch?0:17,0xff,Stacks[0],8);const auto start=f.a->cycles;while(f.a->cycles-start<budget){if(!f.native.active()&&f.a->pending_irqs==0)break;f.step();}f.equalMemory();++f.m.budgets;}
 for(unsigned branch=0;branch<3;++branch){f.prepare(0,branch==2?0xf8:0x91,255,branch?0:17,0xff,Stacks[0],8);f.a->cycles=f.b->cycles=~uint64_t(0)-4;f.whole();}
 f.prepare(0,0x91,255,1,0xff,Stacks[0],8);f.a->pending_irqs=f.b->pending_irqs=(uint64_t(1)<<53)|(uint64_t(1)<<60);f.whole();
 // Pure semantic graph checks exact ordering and all short budgets, without CPU.
 using T=kstation::SerialRxService;using R=T::Read;using W=T::Write;
 for(unsigned data=0;data<256;++data)for(unsigned full=0;full<2;++full){T task;T::Credits credits;unsigned calls=0,dataWrites=0,errorWrites=0,producerWrites=0;bool consumerRead=false;auto read=[&](R field)->unsigned{++calls;if(field==R::Receive)return data;if(field==R::Producer)return 255;if(field==R::Consumer){consumerRead=true;return full?0:17;}return 0xff;};auto write=[&](W field,unsigned value){++calls;if(field==W::Data){check(!consumerRead&&value==data,"RX ring must write before full query");++dataWrites;}if(field==W::Producer){check(!full&&value==0,"RX published full producer");++producerWrites;}if(field==W::Error){check(full&&value==2,"RX overflow latch");++errorWrites;}};
  while(!task.complete()){if(task.awaitingClock()){const auto snapshot=task;check(task.nextClocks(credits)==0&&task.advanceBefore(~0u,credits,read,write)==0&&task.stage()==snapshot.stage(),"Await clock mutated");check(task.resumeClock(),"RX child resume");continue;}const auto cost=task.nextClocks(credits),before=calls;std::array<unsigned char,sizeof(T)>snapshot{};std::memcpy(snapshot.data(),&task,sizeof(task));for(unsigned available=0;available<cost;++available){check(task.advanceBefore(available,credits,read,write)==0&&calls==before&&std::memcmp(snapshot.data(),&task,sizeof(task))==0,"Short RX budget mutated");++f.m.pureRejected;}check(task.advanceBefore(cost,credits,read,write)==cost,"RX phase admission");++f.m.pureIntervals;}
  if(data==0xf8)check(dataWrites==0&&errorWrites==0&&producerWrites==0,"F8 entered normal ring");else check(dataWrites==1&&errorWrites==full&&producerWrites==1-full,"RX queue closure");check(!task.resumeClock()&&task.advanceBefore(~0u,credits,read,write)==0,"Complete RX restarted");}
 for(unsigned vector:{0u,28u,38u,52u,54u,55u,63u}){f.prepare(0,0x91,0,17,0,Stacks[0],0);f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<vector;check(!f.native.step(*f.b).stepped&&f.y.count==0,"Unsupported RX IO mutation");f.equalFrame();++f.m.guards;}
 for(unsigned mode=0;mode<3;++mode){f.prepare(0,0x91,0,17,0,Stacks[0],0);if(mode==0)f.a->pending_irqs_high=f.b->pending_irqs_high=1;if(mode==1)f.a->interruptMode2=f.b->interruptMode2=true;if(mode==2)f.a->ccr=f.b->ccr=0x80;check(!f.native.step(*f.b).stepped&&f.y.count==0,"Blocked RX IO mutation");f.equalFrame();++f.m.guards;}
 watch=false;check(allocations==0,"RX processing allocation");
 f.prepare(0,0x91,0,17,0xff,Stacks[0],8);f.b->memory[0xd7]=0x66;bool changed=false;try{f.native.step(*f.b);}catch(const std::logic_error&){changed=true;}check(changed&&f.native.depth()==0&&f.b->getSP()==Stacks[0]-4&&f.b->pending_irqs==0,"Changed RX vector hidden");++f.m.guards;
 f.prepare(0,0x91,0,17,0xff,Stacks[0],8);f.a->pending_irqs=f.b->pending_irqs=0;f.b->pc=f.b->makepc(0xa70);bool unowned=false;try{f.native.step(*f.b);}catch(const std::logic_error&){unowned=true;}check(unowned&&f.y.count==0,"Unowned RX boundary hidden");++f.m.guards;
 f.prepare(0,0x91,0,17,0xff,Stacks[0],8);for(unsigned n=0;n<8;++n){f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<53;f.step();}check(f.native.depth()==8,"RX capacity fixture");f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<53;f.x.count=f.y.count=0;bool capacity=false;try{f.native.step(*f.b);}catch(const std::logic_error&){capacity=true;}check(capacity&&f.y.count==0&&f.native.depth()==8,"RX capacity hidden");f.equalFrame();++f.m.guards;
 char out[2500];std::snprintf(out,sizeof(out),"{\"passed\":true,\"whole_handlers\":%llu,\"native_rx_phases\":%llu,\"native_clock_phases\":%llu,\"original_clock_sidecar_steps\":%llu,\"ordered_context_io\":%llu,\"native_entries\":%llu,\"native_returns\":%llu,\"clock_child_calls\":%llu,\"live_mutations\":%llu,\"nested_cases\":%llu,\"slice_budgets\":%llu,\"guards\":%llu,\"pure_intervals\":%llu,\"pure_short_budget_checks\":%llu,\"overflow_cases\":%llu,\"processing_allocations\":%llu}\n",(unsigned long long)f.m.handlers,(unsigned long long)f.m.rxPhases,(unsigned long long)f.m.clockPhases,(unsigned long long)f.m.originalClockSidecar,(unsigned long long)f.m.io,(unsigned long long)f.m.entries,(unsigned long long)f.m.returns,(unsigned long long)f.m.clockCalls,(unsigned long long)f.m.mutations,(unsigned long long)f.m.nested,(unsigned long long)f.m.budgets,(unsigned long long)f.m.guards,(unsigned long long)f.m.pureIntervals,(unsigned long long)f.m.pureRejected,(unsigned long long)f.m.overflowCases,(unsigned long long)allocations);std::fputs(out,stdout);if(argc>2){std::ofstream file(argv[2]);file<<out;}return 0;
}catch(const std::exception&e){watch=false;std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
