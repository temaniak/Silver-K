#include "kstationNativeSerialTx.h"
#include "kstationNativeIrq.h"
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
uint32_t seed=0x4b535458;uint32_t randomValue(){seed^=seed<<13;seed^=seed>>17;return seed^=seed<<5;}
class Original final:public H8SEmulator {public:int readHistory()const{return lastread;}int writeHistory()const{return lastwrite;}void histories(int r,int w){lastread=r;lastwrite=w;}int irqDepth()const{return indent;}void irqDepth(int n){indent=n;}};
struct Region{uint32_t at,size;};
constexpr Region Regions[]={{0,16},{0xd8,4},{0xc490,0x10030},{0xfffd00,0x300},{0xfff0,32},{0x1ffff0,32},{0x3ffff0,48}};
constexpr uint32_t Stacks[]={0xffff10,0xfffd12,0x200001,0x3fffff,0x400001,0x1000001,0x100ffff,0xfffe77};
struct Access{uint64_t time,pending,high;uint32_t pc,sp,r0,r1,at;int depth;uint8_t ccr,exr,value;bool write;};
template<class C>class Bus final:public H8SDevice{public:explicit Bus(C&c):cpu(c){}C&cpu;std::array<Access,64>events{};unsigned count=0;
 void record(uint32_t a,uint8_t v,bool w){check(count<events.size(),"IO overflow");events[count++]={cpu.cycles,cpu.pending_irqs,cpu.pending_irqs_high,uint32_t(cpu.getPC()),cpu.getSP(),uint32_t(cpu.reg32(0)),uint32_t(cpu.reg32(1)),a,cpu.irqDepth(),cpu.ccr,cpu.exr,v,w};}
 uint8_t read(uint32_t a)override{const auto v=cpu.memory[a];record(a,v,false);return v;}void write(uint32_t a,uint8_t v)override{record(a,v,true);cpu.memory[a]=v;}
};
struct Metrics{uint64_t handlers=0,phases=0,io=0,entries=0,returns=0,mutations=0,nested=0,budgets=0,guards=0,pureIntervals=0,pureRejected=0;};
class Fixture{public:
 std::unique_ptr<Original>a=std::make_unique<Original>();std::unique_ptr<kstation::Stage1Cpu>b=std::make_unique<kstation::Stage1Cpu>();Bus<Original>x{*a};Bus<kstation::Stage1Cpu>y{*b};kstation::NativeSerialTx native;const std::vector<uint8_t>&rom;Metrics m;
 explicit Fixture(const std::vector<uint8_t>&r):rom(r){for(auto q:Regions){a->memmap(&x,q.at,q.size);b->memmap(&y,q.at,q.size);}a->loadmem(r.data(),unsigned(r.size()),0);b->loadmem(r.data(),unsigned(r.size()),0);}
 void byte(uint32_t at,unsigned v){a->memory[at]=b->memory[at]=uint8_t(v);}
 void word(uint32_t at,unsigned v){byte(at,v>>8);byte(at+1,v);}
 void prepare(unsigned flags,unsigned realtime,unsigned consumer,unsigned producer,unsigned status,uint32_t sp,unsigned ue){
  native.reset();for(auto q:Regions)for(unsigned n=0;n<q.size;++n)a->memory[q.at+n]=b->memory[q.at+n]=uint8_t(n*73+19);
  for(unsigned n=0;n<4;++n)byte(0xd8+n,rom[0xd8+n]);
  for(unsigned n=0;n<8;++n)a->regs[n].er=b->regs[n].er=int32_t(randomValue());
  a->setSP(sp);b->setSP(sp);a->pc=a->makepc(0x3ae8);b->pc=b->makepc(0x3ae8);a->ccr=b->ccr=uint8_t(flags&127);a->exr=b->exr=uint8_t(flags);a->irqDepth(2);b->irqDepth(2);
  a->cycles=b->cycles=uint64_t(randomValue())*3;a->pending_irqs=b->pending_irqs=uint64_t(1)<<54;a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=false;
  a->histories(0x200000,0xfffe72);b->histories(0x200000,0xfffe72);byte(0xfffff2,ue);byte(0xfffe73,realtime);word(0xc494,consumer);word(0xc492,producer);byte(0xffffb4,status);byte(0xc496+consumer,flags^status);x.count=y.count=0;
 }
 void equalFrame(){
  bool ok=a->cycles==b->cycles&&a->getPC()==b->getPC()&&a->ccr==b->ccr&&a->exr==b->exr&&a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->readHistory()==b->readHistory()&&a->writeHistory()==b->writeHistory()&&a->irqDepth()==b->irqDepth();
  for(unsigned n=0;n<8;++n)ok&=a->regs[n].er==b->regs[n].er;
  if(!ok){watch=false;std::fprintf(stderr,"frame PC %x/%x clocks %llu/%llu CCR %x/%x SP %x/%x R0 %x/%x R1 %x/%x\n",a->getPC(),b->getPC(),a->cycles,b->cycles,a->ccr,b->ccr,a->getSP(),b->getSP(),unsigned(a->reg32(0)),unsigned(b->reg32(0)),unsigned(a->reg32(1)),unsigned(b->reg32(1)));throw std::runtime_error("TX full caller frame mismatch");}
  check(x.count==y.count,"TX IO count mismatch");for(unsigned n=0;n<x.count;++n){const auto&p=x.events[n];const auto&q=y.events[n];check(p.time==q.time&&p.pending==q.pending&&p.high==q.high&&p.pc==q.pc&&p.sp==q.sp&&p.r0==q.r0&&p.r1==q.r1&&p.at==q.at&&p.depth==q.depth&&p.ccr==q.ccr&&p.exr==q.exr&&p.value==q.value&&p.write==q.write,"TX live IO context mismatch");}
 }
 void equalMemory(){for(auto q:Regions)check(std::memcmp(a->memory+q.at,b->memory+q.at,q.size)==0,"TX touched RAM/canary mismatch");}
 kstation::NativeSerialTx::Result step(){x.count=y.count=0;const auto old=a->getPC();auto r=native.step(*b);check(r.stepped,"TX native phase rejected");a->step();equalFrame();m.phases++;m.io+=y.count;m.entries+=r.entered;m.returns+=r.returned;(void)old;return r;}
 void whole(bool mutate=false){bool done=false;for(unsigned n=0;n<40&&!done;++n){if(mutate){switch(a->getPC()){case 0xb70:word(0xc492,randomValue());++m.mutations;break;case 0xb9e:byte(uint32_t(a->reg32(1)),randomValue());++m.mutations;break;case 0xba2:byte(0xffffb4,randomValue());++m.mutations;break;default:break;}}done=step().returned;}check(done&&!native.active(),"TX whole handler incomplete");equalMemory();++m.handlers;}
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
 if(argc<2){std::puts("SKIP: private verified K OS required");return 77;}
 std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};check(rom.size()==49728,"ROM size");check(rom[0xd8]==0&&rom[0xd9]==0&&rom[0xda]==0xb&&rom[0xdb]==0x50,"TX vector");Fixture f(rom);watch=true;
 for(unsigned flags=0;flags<256;++flags)for(unsigned branch=0;branch<3;++branch)for(unsigned ue:{0u,8u})for(unsigned sp=0;sp<4;++sp){f.prepare(flags,branch==2?0xf8:0,branch==1?255:0,branch==1?1:0,randomValue(),Stacks[sp],ue);f.whole();}
 for(unsigned rt=1;rt<256;++rt)for(unsigned flags:{0u,0x2fu,0x80u,0xffu})for(unsigned ue:{0u,8u}){f.prepare(flags,rt,randomValue()&255,randomValue()&255,randomValue(),Stacks[rt%8],ue);f.whole();}
 constexpr unsigned Indices[]={0,1,127,128,254,255,256,0x7ffe,0x7fff,0x8000,0xfffe,0xffff};
 for(unsigned c:Indices)for(unsigned p:Indices)for(unsigned flags:{0u,0x2fu,0x80u,0xffu})for(auto sp:Stacks){f.prepare(flags,0,c,p,randomValue(),sp,8);f.whole();}
 for(unsigned status=0;status<256;++status)for(unsigned branch=0;branch<3;++branch){f.prepare(status,branch==2?0xf8:0,5,branch?6:5,status,Stacks[0],0);f.whole(true);}
 // A pending IRQ while masked must read SYSCR before every native phase.
 for(auto sp:Stacks){f.prepare(0x2f,0,255,1,0xff,sp,0);f.step();f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<28;f.whole();}
 // Native TX can resume around a supported, separately owned Timer3 IRQ.
 for(unsigned flags:{0u,0x2fu,0x7fu}){f.prepare(flags,0,255,1,0xff,Stacks[0],8);f.step();f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<38;bool ret=false;for(unsigned n=0;n<12&&!ret;++n){f.x.count=f.y.count=0;const auto r=kstation::NativeIrq::step(*f.b);check(r.stepped,"Nested timer rejected");f.a->step();f.equalFrame();ret=r.returned;}check(ret,"Nested timer return");f.whole();++f.m.nested;}
 // Explicit diagnostic nested TX admissions retain separate task ownership.
 for(unsigned flags:{0u,0x2fu,0x7fu}){f.prepare(flags,0xf8,0,0,0xff,Stacks[0],8);f.step();f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<54;f.step();check(f.native.depth()==2,"Nested TX ownership");unsigned returned=0;for(unsigned n=0;n<70&&returned<2;++n)if(f.step().returned)++returned;check(returned==2&&!f.native.active(),"Nested TX return");f.equalMemory();++f.m.nested;}
 for(unsigned budget:{0u,1u,2u,3u,4u,5u,7u,8u,16u,31u,64u,127u,128u,255u})for(unsigned branch=0;branch<3;++branch){f.prepare(0,branch==2?0xf8:0,255,branch?1:255,0xff,Stacks[0],8);const auto start=f.a->cycles;while(f.a->cycles-start<budget){if(!f.native.active()&&f.m.phases&&f.a->getPC()==0x3ae8&&f.a->pending_irqs==0)break;f.step();}f.equalMemory();++f.m.budgets;}
 // Pure service: all budgets below each phase cost are observational no-ops.
 for(unsigned branch=0;branch<3;++branch)for(unsigned value=0;value<256;++value){kstation::SerialTxService service;kstation::SerialTxService::Credits credits;unsigned calls=0,phases=0;auto read=[&](auto field)->unsigned{++calls;using R=kstation::SerialTxService::Read;switch(field){case R::Realtime:return branch==2?value+1:0;case R::Consumer:return value;case R::Producer:return branch?value+1:value;case R::Status:return value;default:return value;}};auto write=[&](auto,unsigned){++calls;};while(!service.complete()){auto snapshot=service;const auto cost=service.nextClocks(credits),before=calls;for(unsigned budget=0;budget<cost;++budget){check(service.advanceBefore(budget,credits,read,write)==0&&calls==before&&std::memcmp(&snapshot,&service,sizeof(service))==0,"Short TX budget mutated state/IO");++f.m.pureRejected;}check(service.advanceBefore(cost,credits,read,write)==cost,"Admitted pure TX phase");++f.m.pureIntervals;check(++phases<40,"Pure TX did not terminate");}check(service.nextClocks(credits)==0&&service.advanceBefore(~0u,credits,read,write)==0,"Completed TX restarted");}
 // Unsupported/blocked inputs must reject before any CPU or IO mutation.
 for(unsigned vector:{0u,28u,38u,52u,53u,55u,63u}){f.prepare(0,0,0,0,0,Stacks[0],0);f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<vector;f.x.count=f.y.count=0;check(!f.native.step(*f.b).stepped&&f.y.count==0,"Unsupported TX arbiter mutated IO");f.equalFrame();++f.m.guards;}
 for(unsigned mode=0;mode<3;++mode){f.prepare(0,0,0,0,0,Stacks[0],0);if(mode==0)f.a->pending_irqs_high=f.b->pending_irqs_high=1;if(mode==1)f.a->interruptMode2=f.b->interruptMode2=true;if(mode==2)f.a->ccr=f.b->ccr=0x80;check(!f.native.step(*f.b).stepped&&f.y.count==0,"Blocked TX mutated IO");f.equalFrame();++f.m.guards;}

 for(unsigned branch=0;branch<3;++branch){f.prepare(0,branch==2?0xf8:0,255,branch?1:255,0xff,Stacks[0],8);f.a->cycles=f.b->cycles=~uint64_t(0)-4;f.whole();}
 // Priority ties retain higher pending requests through the masked body.
 f.prepare(0,0xf8,255,1,0xff,Stacks[0],8);f.a->pending_irqs=f.b->pending_irqs=(uint64_t(1)<<54)|(uint64_t(1)<<60);f.whole();
 watch=false;check(allocations==0,"TX processing allocation");
 // Fatal contract guards are outside the allocation watch and never silently
 // fall back after stack/vector IO. Capacity rejection occurs BEFORE any IO.
 f.prepare(0,0xf8,0,0,0xff,Stacks[0],8);f.b->memory[0xdb]=0x52;bool changed=false;try{f.native.step(*f.b);}catch(const std::logic_error&){changed=true;}check(changed&&f.native.depth()==0&&f.b->getSP()==Stacks[0]-4&&f.b->pending_irqs==0,"Changed TX vector was hidden");++f.m.guards;
 f.prepare(0,0,0,0,0xff,Stacks[0],8);f.a->pending_irqs=f.b->pending_irqs=0;f.b->pc=f.b->makepc(0xb54);bool unowned=false;try{f.native.step(*f.b);}catch(const std::logic_error&){unowned=true;}check(unowned&&f.y.count==0,"Unowned TX caller was hidden");++f.m.guards;
 f.prepare(0,0xf8,0,0,0xff,Stacks[0],8);for(unsigned n=0;n<8;++n){f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<54;f.step();}check(f.native.depth()==8,"TX capacity fixture");f.a->ccr&=127;f.b->ccr&=127;f.a->pending_irqs=f.b->pending_irqs=uint64_t(1)<<54;f.x.count=f.y.count=0;bool capacity=false;try{f.native.step(*f.b);}catch(const std::logic_error&){capacity=true;}check(capacity&&f.y.count==0&&f.native.depth()==8,"TX capacity was hidden");f.equalFrame();++f.m.guards;

 char out[2048];std::snprintf(out,sizeof(out),"{\"passed\":true,\"whole_handlers\":%llu,\"native_phases\":%llu,\"ordered_context_io\":%llu,\"native_entries\":%llu,\"native_returns\":%llu,\"live_mutations\":%llu,\"nested_cases\":%llu,\"slice_budgets\":%llu,\"guards\":%llu,\"pure_intervals\":%llu,\"pure_short_budget_checks\":%llu,\"processing_allocations\":%llu}\n",(unsigned long long)f.m.handlers,(unsigned long long)f.m.phases,(unsigned long long)f.m.io,(unsigned long long)f.m.entries,(unsigned long long)f.m.returns,(unsigned long long)f.m.mutations,(unsigned long long)f.m.nested,(unsigned long long)f.m.budgets,(unsigned long long)f.m.guards,(unsigned long long)f.m.pureIntervals,(unsigned long long)f.m.pureRejected,(unsigned long long)allocations);std::fputs(out,stdout);if(argc>2){std::ofstream file(argv[2]);file<<out;}return 0;
}catch(const std::exception&e){watch=false;std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
