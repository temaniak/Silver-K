#include "kstationNativeClock.h"
#include "kstationNativeIrq.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <fstream>
#include <vector>
#include <cstring>
#include <new>
#if defined(_MSC_VER)
#include <malloc.h>
#endif
namespace {
bool watch=false;uint64_t allocations=0;void*allocate(size_t n){if(watch)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}uint32_t seed=0x4b434c4b;uint32_t randomValue(){seed^=seed<<13;seed^=seed>>17;return seed^=seed<<5;}
class Original final:public H8SEmulator{public:int readHistory()const{return lastread;}int writeHistory()const{return lastwrite;}void histories(int r,int w){lastread=r;lastwrite=w;}int irqDepth()const{return indent;}void irqDepth(int n){indent=n;}};
struct Region{uint32_t at,size;};constexpr Region Regions[]={{0,16},{0xfffd00,0x300},{0xfff0,32},{0x1ffff0,32},{0x3ffff0,48}};constexpr uint32_t Stacks[]={0xffff00,0xfffdc0,0x200001,0x3fffff,0x400001,0x1000001,0x100ffff};
struct Access{uint64_t time,pending,high;uint32_t pc,sp,r0,r1,r2,at;int depth;uint8_t ccr,exr,value;bool write;};
template<class C>class Bus final:public H8SDevice{public:explicit Bus(C&c):cpu(c){}C&cpu;std::array<Access,64>events{};unsigned count=0;void record(uint32_t a,uint8_t v,bool w){check(count<events.size(),"IO overflow");events[count++]={cpu.cycles,cpu.pending_irqs,cpu.pending_irqs_high,uint32_t(cpu.getPC()),cpu.getSP(),uint32_t(cpu.reg32(0)),uint32_t(cpu.reg32(1)),uint32_t(cpu.reg32(2)),a,cpu.irqDepth(),cpu.ccr,cpu.exr,v,w};}uint8_t read(uint32_t a)override{const auto v=cpu.memory[a];record(a,v,false);return v;}void write(uint32_t a,uint8_t v)override{record(a,v,true);cpu.memory[a]=v;}};
struct Metrics{uint64_t cases=0,steps=0,io=0,updates=0,mutations=0,budgets=0,shortBudgets=0,guards=0,nested=0;};
class Fixture{public:std::unique_ptr<Original>a=std::make_unique<Original>();std::unique_ptr<kstation::Stage1Cpu>b=std::make_unique<kstation::Stage1Cpu>();Bus<Original>x{*a};Bus<kstation::Stage1Cpu>y{*b};kstation::NativeClock clock;Metrics m;
 explicit Fixture(const std::vector<uint8_t>&r){a->loadmem(r.data(),unsigned(r.size()),0);b->loadmem(r.data(),unsigned(r.size()),0);for(auto q:Regions){a->memmap(&x,q.at,q.size);b->memmap(&y,q.at,q.size);}}
 void byte(uint32_t at,uint32_t v){a->memory[at&0xffffff]=b->memory[at&0xffffff]=uint8_t(v);}void word(uint32_t at,uint32_t v){byte(at,v>>8);byte(at+1,v);}void wide(uint32_t at,uint32_t v){word(at,v>>16);word(at+2,v);}
 void prepare(uint32_t entry,unsigned flags,uint32_t sp,unsigned active=0,unsigned source=0){clock.reset();for(auto q:Regions)for(unsigned n=0;n<q.size;++n)byte(q.at+n,n*73+19);for(unsigned n=0;n<8;++n)a->regs[n].er=b->regs[n].er=int32_t(randomValue());a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->setSP(sp);b->setSP(sp);wide(sp,0x3ae8);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=uint8_t(randomValue());a->irqDepth(3);b->irqDepth(3);a->cycles=b->cycles=uint64_t(randomValue())*3;a->pending_irqs=b->pending_irqs=0;a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=false;a->histories(0x20000,0xfffe6e);b->histories(0x20000,0xfffe6e);a->reg8(9)=b->reg8(9)=int8_t(flags&7);a->reg8(10)=b->reg8(10)=int8_t(source);word(0xffff86,0xfff0);word(0xfffd28,8);byte(0xffff85,4);byte(0xfffff2,8);byte(0xfffe6e,active);byte(0xfffe6f,2);byte(0xfffe72,source);word(0xfffe50,20);wide(0xfffe52,0x12345678);wide(0xfffe56,0xabcdef);wide(0xfffe5a,0x12345678);wide(0xfffe5e,0x87654321);word(0xfffe80,19);wide(0xfffe82,10000);word(0xfffe74,512);word(0xfffe76,1000);wide(0xfffe78,0x23456789);word(0xfffe7c,0x800);word(0xfffe7e,0);x.count=y.count=0;++m.cases;}
 void equal(){bool ok=a->cycles==b->cycles&&a->getPC()==b->getPC()&&a->ccr==b->ccr&&a->exr==b->exr&&a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->readHistory()==b->readHistory()&&a->writeHistory()==b->writeHistory()&&a->irqDepth()==b->irqDepth();for(unsigned n=0;n<8;++n)ok&=a->regs[n].er==b->regs[n].er;if(!ok){watch=false;std::fprintf(stderr,"PC %x/%x cycles %llu/%llu CCR %x/%x R0 %x/%x R1 %x/%x R2 %x/%x read %x/%x write %x/%x\n",a->getPC(),b->getPC(),a->cycles,b->cycles,a->ccr,b->ccr,unsigned(a->reg32(0)),unsigned(b->reg32(0)),unsigned(a->reg32(1)),unsigned(b->reg32(1)),unsigned(a->reg32(2)),unsigned(b->reg32(2)),a->readHistory(),b->readHistory(),a->writeHistory(),b->writeHistory());throw std::runtime_error("Clock frame differs");}check(x.count==y.count,"Clock IO count differs");for(unsigned n=0;n<x.count;++n){const auto&p=x.events[n];const auto&q=y.events[n];bool ok=p.time==q.time&&p.pending==q.pending&&p.high==q.high&&p.pc==q.pc&&p.sp==q.sp&&p.r0==q.r0&&p.r1==q.r1&&p.r2==q.r2&&p.at==q.at&&p.depth==q.depth&&p.ccr==q.ccr&&p.exr==q.exr&&p.value==q.value&&p.write==q.write;if(!ok){watch=false;std::fprintf(stderr,"IO%u at %x/%x value %x/%x clock %llu/%llu pc%x/%x SP%x/%x flags%x/%x\n",n,p.at,q.at,p.value,q.value,p.time,q.time,p.pc,q.pc,p.sp,q.sp,p.ccr,q.ccr);throw std::runtime_error("Clock live context differs");}}}
 void step(){x.count=y.count=0;const auto at=a->getPC();if(at==0x1aba)++m.updates;try{check(clock.step(*b),"Native clock rejected phase");}catch(...){a->step();watch=false;std::fprintf(stderr,"Failure at %x original costend%llu nativecostend%llu\n",at,a->cycles,b->cycles);throw;}a->step();equal();++m.steps;m.io+=y.count;}
 void whole(bool mutate=false){for(unsigned n=0;n<256&&a->getPC()!=0x3ae8;++n){if(mutate){switch(a->getPC()){case 0x1994:byte(0xfffe6f,randomValue());++m.mutations;break;case 0x19ca:byte(0xfffe6f,randomValue());++m.mutations;break;case 0x1ad4:case 0x1b0a:case 0x1b38:word(0xfffe7c,randomValue());++m.mutations;break;default:break;}}step();}check(a->getPC()==0x3ae8&&!clock.active(),"Clock graph did not return");for(auto q:Regions)check(std::memcmp(a->memory+q.at,b->memory+q.at,q.size)==0,"Clock RAM/canary differs");}
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
 for(unsigned flags=0;flags<256;++flags)for(unsigned branch=0;branch<3;++branch){f.prepare(0x1970,flags,Stacks[0]);f.wide(0xfffe52,branch?0xffffffffu:0);f.wide(0xfffe56,branch?1:0);f.byte(0xfffe6f,branch==2?2:0);f.byte(0xfffe72,flags);f.whole(true);}
 for(unsigned flags=0;flags<256;++flags)for(unsigned active:{0u,1u})for(unsigned source:{0u,254u,255u}){f.prepare(0x19f4,flags,Stacks[0],active,source);f.whole();}
 constexpr unsigned Gains[]={0,1,0x7f,0x80,0x7ff,0x800,0x4000,0x7fff,0x8000,0xffff};
 constexpr unsigned Samples[]={0,1,0x7dff,0x7e00,0x7fff,0x8000,0xfdff,0xfe00,0xffff};
 for(unsigned gain:Gains)for(unsigned samples:Samples)for(unsigned flags:{0u,0x2fu,0x80u,0xffu})for(unsigned source:{0u,1u,254u}){f.prepare(0x19f4,flags,Stacks[0],1,source);f.word(0xfffe7c,gain);f.word(0xfffe74,samples);f.word(0xfffe7e,randomValue());f.whole();}
 for(auto sp:Stacks)for(unsigned entry:{0x946u,0xaf4u,0x1970u,0x19f4u,0x1a08u})for(unsigned flags:{0u,0x2fu,0x80u,0xffu}){f.prepare(entry,flags,sp,1,0);f.whole(true);}
 for(unsigned counter:{0u,1u,999u,1000u,1001u,0x7fffu,0x8000u,0xffffu})for(unsigned ovf:{0u,0x7fffu,0x8000u,0xffffu})for(unsigned flag:{0u,4u}){f.prepare(0x946,0xff,Stacks[0]);f.word(0xffff86,counter);f.word(0xfffd28,ovf);f.byte(0xffff85,flag);f.whole();}
 for(unsigned elapsed:{0u,1u,19999u,20000u,20001u,32767u,32768u,65535u}){f.prepare(0x19f4,0xff,Stacks[0],1,0);f.word(0xfffe76,uint16_t(0x8fff-elapsed));f.whole(true);}
 for(unsigned entry:{0x1970u,0x19f4u}){f.prepare(entry,0xff,Stacks[0],1,0);f.a->cycles=f.b->cycles=~uint64_t(0)-4;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<38;f.whole();}

 // Every pure phase rejects every shorter budget before callbacks/state changes.
 using P=kstation::ClockPulseTask;using T=kstation::ClockTickTask;using R=kstation::ClockReadTask;using W=kstation::ClockSendTask;
 for(unsigned kind=0;kind<2;++kind)for(unsigned active=0;active<2;++active)for(unsigned gain:{0x80u,0x800u,0x4000u,0x8000u}){
  P task(kind?P::Kind::Edge:P::Kind::Latched,3,0);P::Credits cr;unsigned calls=0,phases=0;
  auto read=[&](P::Read f)->uint32_t{++calls;switch(f){case P::Read::Active:return active;case P::Read::Latched:return 0;case P::Read::Samples:return 512;case P::Read::Gain:return gain;case P::Read::Timer:return 20u<<16;case P::Read::PulseCount:return 19;case P::Read::Sum:return 10000;case P::Read::PreviousTick:return 1000;case P::Read::Estimate:return 0x23456789;default:return 0;}};auto write=[&](auto,auto){++calls;};
  while(!task.complete()){if(task.awaitingChild()){check(task.resumeChild(0x8fff),"Pulse child resume");continue;}const auto cost=task.nextClocks(cr),before=calls;std::array<unsigned char,sizeof(task)>snapshot;std::memcpy(snapshot.data(),&task,sizeof(task));for(unsigned budget=0;budget<cost;++budget){check(task.advanceBefore(budget,cr,read,write)==0&&calls==before&&std::memcmp(snapshot.data(),&task,sizeof(task))==0,"Pulse short budget mutated state/IO");++f.m.shortBudgets;}check(task.advanceBefore(cost,cr,read,write)==cost,"Pulse phase admission");++f.m.budgets;check(++phases<160,"Pure pulse did not finish");}
 }
 for(unsigned branch=0;branch<3;++branch){T task;kstation::ClockCredits cr;unsigned calls=0,phases=0;auto read=[&](T::Read field)->uint32_t{++calls;switch(field){case T::Read::Increment:return branch?1:0;case T::Read::Phase:return branch?0xffffffffu:0;case T::Read::OutputFlags:return branch==2?2:0;case T::Read::Timeout:return branch?255:0;default:return 0;}};auto write=[&](auto,auto){++calls;};while(!task.complete()){if(task.awaitingChild()){task.resumeChild();continue;}const auto cost=task.nextClocks(cr),before=calls;std::array<unsigned char,sizeof(task)>copy;std::memcpy(copy.data(),&task,sizeof(task));for(unsigned b=0;b<cost;++b){check(task.advanceBefore(b,cr,read,write)==0&&calls==before&&std::memcmp(copy.data(),&task,sizeof(task))==0,"Tick short budget mutated state/IO");++f.m.shortBudgets;}check(task.advanceBefore(cost,cr,read,write)==cost,"Tick phase admission");++f.m.budgets;check(++phases<80,"Pure tick did not finish");}}
 for(unsigned counter:{0u,1000u,1001u,65535u}){R task;kstation::ClockCredits cr;unsigned calls=0;auto read=[&](R::Read field){++calls;return field==R::Read::Counter?counter:field==R::Read::Flag?4u:65535u;};while(!task.complete()){const auto cost=task.nextClocks(cr),before=calls;std::array<unsigned char,sizeof(task)>copy;std::memcpy(copy.data(),&task,sizeof(task));for(unsigned b=0;b<cost;++b){check(task.advanceBefore(b,cr,read)==0&&calls==before&&std::memcmp(copy.data(),&task,sizeof(task))==0,"Counter short budget mutated state/IO");++f.m.shortBudgets;}check(task.advanceBefore(cost,cr,read)==cost,"Counter phase admission");++f.m.budgets;}}
 {W task;kstation::ClockCredits cr;unsigned calls=0;auto write=[&](auto,auto){++calls;};while(!task.complete()){const auto cost=task.nextClocks(cr),before=calls;std::array<unsigned char,sizeof(task)>copy;std::memcpy(copy.data(),&task,sizeof(task));for(unsigned b=0;b<cost;++b){check(task.advanceBefore(b,cr,write)==0&&calls==before&&std::memcmp(copy.data(),&task,sizeof(task))==0,"Send short budget mutated state/IO");++f.m.shortBudgets;}check(task.advanceBefore(cost,cr,write)==cost,"Send phase admission");++f.m.budgets;}}
 // Clock helper can pause around independently owned mode0 Timer3 and resume.
 for(unsigned entry:{0x1970u,0x19f4u,0x1a08u}){f.prepare(entry,0xff,Stacks[0],1,0);f.step();f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<38;f.x.count=f.y.count=0;check(!f.clock.step(*f.b)&&f.y.count==0,"Unmasked clock must yield without IO");f.equal();bool returned=false;for(unsigned n=0;n<12&&!returned;++n){f.x.count=f.y.count=0;const auto result=kstation::NativeIrq::step(*f.b);check(result.stepped,"Nested timer native reject");f.a->step();f.equal();returned=result.returned;}check(returned,"Nested timer incomplete");f.whole();++f.m.nested;}
 // Supported entries are still read-only rejects under foreign interrupt modes.
 for(unsigned mode=0;mode<3;++mode){f.prepare(0x1970,0,Stacks[0]);if(mode==0)f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<53;if(mode==1)f.a->pending_irqs_high=f.b->pending_irqs_high=1;if(mode==2)f.a->interruptMode2=f.b->interruptMode2=true;check(!f.clock.step(*f.b)&&!f.clock.active()&&f.y.count==0,"Clock blocked guard mutated state/IO");f.equal();++f.m.guards;}
 watch=false;check(allocations==0,"Clock allocated");char report[2048];std::snprintf(report,sizeof(report),"{\"passed\":true,\"graphs\":%llu,\"native_phases\":%llu,\"ordered_context_io\":%llu,\"adaptive_updates\":%llu,\"live_mutations\":%llu,\"pure_intervals\":%llu,\"short_budget_checks\":%llu,\"nested_cases\":%llu,\"guards\":%llu,\"processing_allocations\":%llu}\n",(unsigned long long)f.m.cases,(unsigned long long)f.m.steps,(unsigned long long)f.m.io,(unsigned long long)f.m.updates,(unsigned long long)f.m.mutations,(unsigned long long)f.m.budgets,(unsigned long long)f.m.shortBudgets,(unsigned long long)f.m.nested,(unsigned long long)f.m.guards,(unsigned long long)allocations);std::fputs(report,stdout);if(argc>2){std::ofstream out(argv[2]);out<<report;}return 0;
}catch(const std::exception&e){watch=false;std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
