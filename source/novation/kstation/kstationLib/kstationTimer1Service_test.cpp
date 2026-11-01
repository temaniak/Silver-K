#include "kstationNativeTimer1.h"
#ifndef KSTATION_TIMER1_ORIGINAL_CLOCK_CHILD
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
bool watch=false;uint64_t allocations=0;void*allocate(size_t n){if(watch)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void check(bool ok,const char*w){if(!ok)throw std::runtime_error(w);}
uint32_t seed=0x54494d31;uint32_t randomValue(){seed^=seed<<13;seed^=seed>>17;return seed^=seed<<5;}
class Original final:public H8SEmulator{public:int readHistory()const{return lastread;}int writeHistory()const{return lastwrite;}void histories(int r,int w){lastread=r;lastwrite=w;}int irqDepth()const{return indent;}void irqDepth(int n){indent=n;}};
struct Region{uint32_t at,size;};constexpr Region Regions[]={{0x70,4},{0xa850,0x120},{0xc6aa,0x10210},{0xfffd00,0x300},{0,16},{0xfff0,32},{0x1ffff0,32},{0x3ffff0,48}};constexpr uint32_t Stacks[]={0xffff10,0xfffd12,0x200001,0x3fffff,0x400001,0x1000001,0x100ffff};
struct Access{uint64_t time,pending,high;uint32_t pc,sp;std::array<int32_t,8>reg;int depth;uint32_t at;uint8_t ccr,exr,v;bool write;};
template<class C>class Bus final:public H8SDevice{public:explicit Bus(C&c):cpu(c){}C&cpu;std::array<Access,64>events{};unsigned count=0,pinReads=0,lcdReads=0,busy=0;std::array<uint8_t,8>pins{};
 void record(uint32_t a,uint8_t v,bool w){check(count<events.size(),"Timer1 IO capacity");auto&x=events[count++];x.time=cpu.cycles;x.pending=cpu.pending_irqs;x.high=cpu.pending_irqs_high;x.pc=uint32_t(cpu.getPC());x.sp=cpu.getSP();for(unsigned n=0;n<8;++n)x.reg[n]=cpu.regs[n].er;x.depth=cpu.irqDepth();x.at=a;x.ccr=cpu.ccr;x.exr=cpu.exr;x.v=v;x.write=w;}
 uint8_t read(uint32_t a)override{const auto v=a==0xffffd7?pins[pinReads++%8]:a==0xffffc7?uint8_t(lcdReads++<busy?0x80:0):cpu.memory[a];record(a,v,false);return v;}void write(uint32_t a,uint8_t v)override{record(a,v,true);cpu.memory[a]=v;}
};
struct Metrics{uint64_t handlers=0,phases=0,io=0,entries=0,returns=0,clockOriginal=0,clockNative=0,mutations=0,budgets=0,guards=0,pureRejects=0,pureStages=0,clockWaits=0,nested=0,transitions=0,panel=0;};
class Fixture{public:std::unique_ptr<Original>a=std::make_unique<Original>();std::unique_ptr<kstation::Stage1Cpu>b=std::make_unique<kstation::Stage1Cpu>();Bus<Original>x{*a};Bus<kstation::Stage1Cpu>y{*b};kstation::NativeTimer1 native;
#ifndef KSTATION_TIMER1_ORIGINAL_CLOCK_CHILD
 kstation::NativeClock clock;
#endif
 const std::vector<uint8_t>&rom;Metrics m;explicit Fixture(const std::vector<uint8_t>&r):rom(r){for(auto q:Regions){a->memmap(&x,q.at,q.size);b->memmap(&y,q.at,q.size);}a->loadmem(r.data(),unsigned(r.size()),0);b->loadmem(r.data(),unsigned(r.size()),0);}
 void byte(uint32_t at,unsigned v){a->memory[at]=b->memory[at]=uint8_t(v);}void word(uint32_t at,unsigned v){byte(at,v>>8);byte(at+1,v);}void lng(uint32_t at,uint32_t v){word(at,v>>16);word(at+2,v);}
 void prepare(unsigned flags,uint32_t sp,unsigned ue,unsigned divider=0,uint32_t tick=0){native.reset();
#ifndef KSTATION_TIMER1_ORIGINAL_CLOCK_CHILD
 clock.reset();
#endif
 for(auto q:Regions)for(unsigned n=0;n<q.size;++n)a->memory[q.at+n]=b->memory[q.at+n]=uint8_t(n*73+19);for(unsigned n=0;n<4;++n)byte(0x70+n,rom[0x70+n]);
 for(unsigned n=0;n<8;++n)a->regs[n].er=b->regs[n].er=int32_t(randomValue());a->setSP(sp);b->setSP(sp);a->pc=a->makepc(0x3ae8);b->pc=b->makepc(0x3ae8);a->ccr=b->ccr=uint8_t(flags&127);a->exr=b->exr=uint8_t(flags);a->irqDepth(1);b->irqDepth(1);a->cycles=b->cycles=uint64_t(randomValue())*3;a->pending_irqs=b->pending_irqs=uint64_t(1)<<28;a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=false;a->histories(0x200000,0xfffe8e);b->histories(0x200000,0xfffe8e);
 for(uint32_t p=0xa856;p<0xa96a;++p)byte(p,0);for(unsigned q=0;q<32;++q){byte(0xa86a+4*q+1,q);word(0xa86a+4*q+2,0x1234);}for(uint32_t p=0xfffe50;p<0xfffe86;++p)byte(p,0);word(0xfffe86,0x2345);byte(0xfffff2,ue);byte(0xfffd2a,divider);lng(0xfffd20,tick);for(unsigned n=0;n<3;++n)byte(0xfffe8f+n,0);x.pins.fill(0);y.pins.fill(0);x.count=y.count=x.pinReads=y.pinReads=x.lcdReads=y.lcdReads=x.busy=y.busy=0;word(0xc6aa,0);}
 void equal(){bool ok=a->cycles==b->cycles&&a->getPC()==b->getPC()&&a->ccr==b->ccr&&a->exr==b->exr&&a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->readHistory()==b->readHistory()&&a->writeHistory()==b->writeHistory()&&a->irqDepth()==b->irqDepth()&&a->interruptMode2==b->interruptMode2;for(unsigned n=0;n<8;++n)ok&=a->regs[n].er==b->regs[n].er;if(!ok){watch=false;std::fprintf(stderr,"Timer1 frame PC%x/%x clocks%llu/%llu CCR%x/%x SP%x/%x R0%x/%x R1%x/%x R2%x/%x R3%x/%x R4%x/%x R5%x/%x\n",a->getPC(),b->getPC(),a->cycles,b->cycles,a->ccr,b->ccr,a->getSP(),b->getSP(),unsigned(a->reg32(0)),unsigned(b->reg32(0)),unsigned(a->reg32(1)),unsigned(b->reg32(1)),unsigned(a->reg32(2)),unsigned(b->reg32(2)),unsigned(a->reg32(3)),unsigned(b->reg32(3)),unsigned(a->reg32(4)),unsigned(b->reg32(4)),unsigned(a->reg32(5)),unsigned(b->reg32(5)));throw std::runtime_error("Timer1 caller frame");}
 check(x.count==y.count,"Timer1 IO count");for(unsigned n=0;n<x.count;++n){const auto&p=x.events[n];const auto&q=y.events[n];if(!(p.time==q.time&&p.pending==q.pending&&p.high==q.high&&p.pc==q.pc&&p.sp==q.sp&&p.reg==q.reg&&p.depth==q.depth&&p.at==q.at&&p.ccr==q.ccr&&p.exr==q.exr&&p.v==q.v&&p.write==q.write)){watch=false;std::fprintf(stderr,"IO mismatch at phase PC%x event%u addr%x/%x clocks%llu/%llu ccr%x/%x\n",a->getPC(),n,p.at,q.at,p.time,q.time,p.ccr,q.ccr);throw std::runtime_error("Timer1 live IO context");}}}
 void memory(){for(auto q:Regions)check(std::memcmp(a->memory+q.at,b->memory+q.at,q.size)==0,"Timer1 RAM/canary");}
 bool step(){x.count=y.count=0;const auto pc=b->getPC();auto r=native.step(*b);if(r.stepped){++m.phases;m.entries+=r.entered;m.returns+=r.returned;}else{
#ifdef KSTATION_TIMER1_ORIGINAL_CLOCK_CHILD
 b->step();++m.clockOriginal;
#else
 check(clock.step(*b),"Timer1 closed native graph rejected");++m.clockNative;
#endif
 }a->step();equal();m.io+=y.count;if(pc>=0x1d34&&pc<0x1d54)++m.transitions;if(pc==0x2186)++m.panel;return r.returned;}
 void whole(bool mutate=false){bool done=false;for(unsigned n=0;n<10000&&!done;++n){if(mutate&&a->getPC()==0x1e7c){word(0xc6aa,randomValue()&0x1fe);++m.mutations;}done=step();}check(done&&!native.active(),"Timer1 whole not complete");memory();++m.handlers;}
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

int main(int argc,char**argv){try{if(argc<2){std::puts("SKIP private K OS required");return 77;}std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};check(rom.size()==49728,"ROM size");Fixture f(rom);watch=true;
 for(unsigned flags:{0u,0x2fu,0x7fu})for(unsigned ue:{0u,8u})for(auto sp:Stacks){f.prepare(flags,sp,ue);f.whole();}
#ifndef KSTATION_TIMER1_SMOKE
 constexpr unsigned delays[]={0,1,2,3,9,10,11,29,30,119,120,0x7fff,0x8000,0xffff};
 for(unsigned group=0;group<8;++group)for(unsigned q=0;q<4;++q)for(unsigned transition=0;transition<16;++transition)for(unsigned mode=0;mode<4;++mode)for(unsigned delay:delays){
  f.prepare(mode*0x2f,Stacks[(group+q+mode)%7],mode&1?8:0);const auto record=0xa86a+group*32+q*4;f.byte(record,mode);f.word(0xfffe86,uint16_t(0x1234+delay-1));const auto cp=((transition>>3)&1)|(((transition>>2)&1)<<1),pp=((transition>>1)&1)|((transition&1)<<1);const auto other=(q+1)%4;const auto current=uint8_t((cp<<(2*q))|(1u<<(2*other))),previous=uint8_t(pp<<(2*q));f.byte(0xa856+group,previous);f.x.pins[group]=f.y.pins[group]=current;f.word(0xc6aa,(delay&0x1fe));f.whole(mode==3);
 }
 for(unsigned a:{0u,1u,2u,127u,128u,255u})for(unsigned b:{0u,1u,2u,255u})for(unsigned c:{0u,1u,2u,255u})for(unsigned busy:{0u,1u,3u,9u}){f.prepare(a,Stacks[(a+b+c)%7],8,128,7);f.byte(0xfffe8f,a);f.byte(0xfffe90,b);f.byte(0xfffe91,c);f.byte(0xfffe8e,a^b);f.byte(0xffffd2,b^c);f.x.busy=f.y.busy=busy;f.whole();}
 for(unsigned budget:{0u,1u,2u,3u,4u,5u,7u,8u,16u,31u,64u,127u,128u,255u}){f.prepare(0,Stacks[0],8);const auto start=f.a->cycles;while(f.a->cycles-start<budget)if(f.step())break;f.memory();++f.m.budgets;}
 for(unsigned v:{0u,12u,27u,29u,38u,52u,53u,54u,60u}){f.prepare(0,Stacks[0],8);f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<v;check(!f.native.step(*f.b).stepped&&f.y.count==0,"Timer1 unsupported guard");f.equal();++f.m.guards;}
 f.prepare(0,Stacks[0],8);f.step();f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<60;f.whole();
#endif


 // Internal clock carry and outgoing F8 generation are a real native child.
 for(unsigned flags:{0u,0x2fu,0x7fu})for(unsigned ue:{0u,8u}){f.prepare(flags,Stacks[0],ue,128,7);f.lng(0xfffe52,0xfffffff0);f.lng(0xfffe56,0x80);f.word(0xfffe50,0xffff);f.byte(0xfffe6f,3);f.byte(0xfffe72,255);f.whole();check(f.b->memory[0xfffe73]==0xf8,"Timer1 clock child did not queue F8");}
 // Nested Timer1 calls retain independent typed graph ownership.
 for(unsigned flags:{0u,0x2fu,0x7fu}){f.prepare(flags,Stacks[0],8);for(unsigned n=0;n<20;++n)f.step();f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<28;f.step();unsigned ret=0;for(unsigned n=0;n<10000&&ret<2;++n)if(f.step())++ret;check(ret==2&&!f.native.active(),"Nested Timer1 graph incomplete");f.memory();++f.m.nested;}
 // Pure service query/short-budget/blocked-child contracts, with no CPU frame.
 std::array<bool,unsigned(kstation::Timer1Service::Stage::Complete)>seen{};
 for(unsigned action=0;action<16;++action)for(unsigned mode=0;mode<4;++mode)for(unsigned delay:{0u,1u,3u,10u,120u,512u,0xffffu}){
  using T=kstation::Timer1Service;using R=T::Read;T task;T::Credits credits{10,3,3};unsigned calls=0,lcd=0,steps=0;const unsigned cp=((action>>3)&1)|(((action>>2)&1)<<1),pp=((action>>1)&1)|((action&1)<<1);const auto current=uint8_t(cp|16),previous=uint8_t(pp);auto read=[&](R r,uint32_t)->uint32_t{++calls;switch(r){case R::Divider:return 128;case R::Tick:return 7;case R::ScanTick:return 0x1233+delay;case R::Pins:return current;case R::History:return previous;case R::EventTick:return uint16_t(0x1234+delay);case R::EventStart:return 0x1234;case R::EventKey:return 17;case R::RecordFlagsKeep:case R::RecordFlagsTest:case R::SpeedFlags:return mode;case R::Producer:return delay&0xffff;case R::PanelCount:return 1;case R::PanelPort:return 0xff;case R::PanelMirror:return 0x30;case R::LcdData:return lcd++<2?0x80:0;default:return 0;}};auto write=[&](auto,uint32_t,uint32_t){++calls;};
  while(!task.complete()){check(++steps<10000,"Pure Timer1 did not finish");if(task.awaitingClock()){std::array<unsigned char,sizeof(task)>snapshot;std::memcpy(snapshot.data(),&task,sizeof(task));const auto before=calls;check(task.nextClocks(credits)==0&&task.advanceBefore(~0u,credits,read,write)==0&&calls==before&&!std::memcmp(snapshot.data(),&task,sizeof(task)),"Blocked clock child mutated Timer1");check(task.resumeClock()&&!task.resumeClock(),"Timer1 clock child resume contract");++f.m.clockWaits;}
   seen[unsigned(task.stage())]=true;const auto cost=task.nextClocks(credits),before=calls;check(cost>0,"Unblocked pure Timer1 has no phase");std::array<unsigned char,sizeof(task)>snapshot;std::memcpy(snapshot.data(),&task,sizeof(task));for(unsigned budget=0;budget<cost;++budget){check(!task.advanceBefore(budget,credits,read,write)&&calls==before&&!std::memcmp(snapshot.data(),&task,sizeof(task)),"Short Timer1 budget changed data/IO");++f.m.pureRejects;}check(task.advanceBefore(cost,credits,read,write)==cost,"Pure Timer1 admission differs");
  }check(!task.nextClocks(credits)&&!task.advanceBefore(~0u,credits,read,write)&&!task.resumeClock(),"Completed Timer1 restarted");
 }
 for(auto v:seen)f.m.pureStages+=v;

 // Explicit blocked modes remain observational no-ops before admission.
 for(unsigned kind=0;kind<3;++kind){f.prepare(0,Stacks[0],8);if(kind==0)f.a->interruptMode2=f.b->interruptMode2=true;if(kind==1)f.a->pending_irqs_high=f.b->pending_irqs_high=1;if(kind==2)f.a->ccr=f.b->ccr=0x80;check(!f.native.step(*f.b).stepped&&f.y.count==0,"Timer1 blocked mode changed state");f.equal();++f.m.guards;}
 watch=false;check(allocations==0,"Timer1 processing allocation");
 // Fatal contract checks are intentionally outside the allocation watch.
 f.prepare(0,Stacks[0],8);f.b->memory[0x73]=0x6a;bool bad=false;try{f.native.step(*f.b);}catch(const std::logic_error&){bad=true;}check(bad&&f.b->getSP()==Stacks[0]-4&&f.b->pending_irqs==0&&!f.native.active(),"Timer1 changed vector hidden");++f.m.guards;
 f.prepare(0,Stacks[0],8);for(unsigned n=0;n<8;++n){f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<28;f.step();}f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<28;f.x.count=f.y.count=0;bool cap=false;try{f.native.step(*f.b);}catch(const std::logic_error&){cap=true;}check(cap&&f.y.count==0,"Timer1 capacity guard changed IO");f.equal();++f.m.guards;
char out[2048];std::snprintf(out,sizeof(out),"{\"passed\":true,\"whole_handlers\":%llu,\"native_phases\":%llu,\"ordered_context_io\":%llu,\"native_entries\":%llu,\"native_returns\":%llu,\"clock_original_steps\":%llu,\"clock_native_steps\":%llu,\"transitions\":%llu,\"panel_calls\":%llu,\"live_mutations\":%llu,\"slice_budgets\":%llu,\"guards\":%llu,\"pure_short_budget_checks\":%llu,\"pure_stages\":%llu,\"clock_wait_guards\":%llu,\"nested_cases\":%llu,\"processing_allocations\":%llu}\n",(unsigned long long)f.m.handlers,(unsigned long long)f.m.phases,(unsigned long long)f.m.io,(unsigned long long)f.m.entries,(unsigned long long)f.m.returns,(unsigned long long)f.m.clockOriginal,(unsigned long long)f.m.clockNative,(unsigned long long)f.m.transitions,(unsigned long long)f.m.panel,(unsigned long long)f.m.mutations,(unsigned long long)f.m.budgets,(unsigned long long)f.m.guards,(unsigned long long)f.m.pureRejects,(unsigned long long)f.m.pureStages,(unsigned long long)f.m.clockWaits,(unsigned long long)f.m.nested,(unsigned long long)allocations);std::fputs(out,stdout);if(argc>2){std::ofstream file(argv[2]);file<<out;}return 0;}catch(const std::exception&e){watch=false;std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
