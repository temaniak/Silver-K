#include "kstationNativeIrq.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {
bool watch=false;
std::uint64_t allocations=0;
void* allocate(std::size_t n){if(watch)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::uint64_t rng=0x6b2d697271323032ull;
std::uint32_t randomValue(){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return static_cast<std::uint32_t>(rng);}
class ReferenceCpu final:public H8SEmulator {
public:
 int readHistory()const{return lastread;}int writeHistory()const{return lastwrite;}
 void histories(int r,int w){lastread=r;lastwrite=w;}
 int irqDepth()const{return indent;}void irqDepth(int n){indent=n;}
};
struct Frame {
 std::array<std::int32_t,8> regs{};
 std::uint64_t cycles=0,pending=0,high=0;
 int pc=0,depth=0,read=0,write=0;
 std::uint8_t ccr=0,exr=0;
 bool mode2=false;
};
template<class Cpu>Frame frame(const Cpu&c){Frame f;for(unsigned n=0;n<8;++n)f.regs[n]=c.regs[n].er;f.cycles=c.cycles;f.pending=c.pending_irqs;f.high=c.pending_irqs_high;f.pc=c.getPC();f.depth=c.irqDepth();f.read=c.readHistory();f.write=c.writeHistory();f.ccr=c.ccr;f.exr=c.exr;f.mode2=c.interruptMode2;return f;}
bool sameFrame(const Frame&a,const Frame&b){return a.regs==b.regs&&a.cycles==b.cycles&&a.pending==b.pending&&a.high==b.high&&a.pc==b.pc&&a.depth==b.depth&&a.read==b.read&&a.write==b.write&&a.ccr==b.ccr&&a.exr==b.exr&&a.mode2==b.mode2;}
struct Access {
 std::uint64_t clock,pending,high;
 std::uint32_t pc,sp,r0,address;
 int depth;
 std::uint8_t ccr,exr,value;
 bool write;
};
template<class Cpu>class Audit final:public H8SDevice {
public:
 explicit Audit(Cpu&c):cpu(c){}
 Cpu&cpu;std::array<Access,64> log{};unsigned count=0;
 void record(std::uint32_t address,std::uint8_t value,bool write){check(count<log.size(),"IO log capacity");log[count++]={cpu.cycles,cpu.pending_irqs,cpu.pending_irqs_high,static_cast<std::uint32_t>(cpu.getPC()),cpu.getSP(),static_cast<std::uint32_t>(cpu.regs[0].er),address,cpu.irqDepth(),cpu.ccr,cpu.exr,value,write};}
 std::uint8_t read(std::uint32_t a)override{const auto v=cpu.memory[a];record(a,v,false);return v;}
 void write(std::uint32_t a,std::uint8_t v)override{record(a,v,true);cpu.memory[a]=v;}
};
struct Region {std::uint32_t begin,size;};
constexpr std::array<Region,7> Regions{{{0xfffd00,0x300},{0,16},{0x98,4},{0xd0,4},{0xfff0,32},{0x1ffff0,32},{0x3ffff0,48}}};
constexpr std::array<std::uint32_t,8> Stacks{{0xffff10,0xfffd12,0x200001,0x3fffff,0x400001,0x1000001,0x100ffff,0xfffd2e}};
constexpr std::array<std::uint16_t,11> Words{{0,1,0x7f,0x80,0xff,0x7ffe,0x7fff,0x8000,0x8001,0xfffe,0xffff}};
constexpr std::array<std::uint32_t,17> Phases{{0x92c,0x92e,0x930,0x932,0x934,0x93a,0x93c,0x942,0x944,0xa50,0xa52,0xa54,0xa56,0xa58,0xa5a,0xa60,0xa62}};
struct Metrics {std::uint64_t cases=0,steps=0,nativeSteps=0,admissions=0,returns=0,fallbacks=0,io=0,syscrReads=0,guards=0,slices=0,whole=0,nested=0;};
class Fixture {
public:
 explicit Fixture(const std::vector<std::uint8_t>&image):rom(image),ref(std::make_unique<ReferenceCpu>()),native(std::make_unique<kstation::Stage1Cpu>()),a(*ref),b(*native){
  ref->loadmem(image.data(),static_cast<unsigned>(image.size()),0);native->loadmem(image.data(),static_cast<unsigned>(image.size()),0);
  for(auto r:Regions){ref->memmap(&a,r.begin,r.size);native->memmap(&b,r.begin,r.size);}
 }
 const std::vector<std::uint8_t>&rom;
 std::unique_ptr<ReferenceCpu>ref;std::unique_ptr<kstation::Stage1Cpu>native;
 Audit<ReferenceCpu>a;Audit<kstation::Stage1Cpu>b;Metrics metrics;
 void prepare(std::uint32_t pc,unsigned flags,unsigned word,unsigned status,std::uint32_t sp,unsigned syscr,std::uint64_t pending=0){
  for(auto r:Regions)for(unsigned n=0;n<r.size;++n)ref->memory[r.begin+n]=native->memory[r.begin+n]=static_cast<std::uint8_t>(n*73+19);
  for(unsigned vector:{38u,52u})for(unsigned n=0;n<4;++n)ref->memory[vector*4+n]=native->memory[vector*4+n]=rom[vector*4+n];
  for(unsigned n=0;n<8;++n)ref->regs[n].er=native->regs[n].er=static_cast<std::int32_t>(randomValue());
  ref->setSP(sp);native->setSP(sp);ref->reg16(0)=native->reg16(0)=static_cast<std::int16_t>(word);
  ref->pc=ref->makepc(pc);native->pc=native->makepc(pc);ref->ccr=native->ccr=static_cast<std::uint8_t>(flags);
  ref->exr=native->exr=static_cast<std::uint8_t>(randomValue());ref->cycles=native->cycles=std::uint64_t(randomValue())*3;
  const int history=static_cast<std::int32_t>(randomValue());ref->histories(history,history/2);native->histories(history,history/2);
  ref->irqDepth(3);native->irqDepth(3);ref->pending_irqs=native->pending_irqs=pending;
  ref->pending_irqs_high=native->pending_irqs_high=0;ref->interruptMode2=native->interruptMode2=false;
  byte(0xfffff2,syscr);byte(0xffff85,status);byte(0xffffb4,status);byte(0xfffd28,word>>8);byte(0xfffd29,word);byte(0xfffe30,0xac);
  a.count=b.count=0;++metrics.cases;
 }
 void byte(std::uint32_t at,unsigned value){ref->memory[at]=native->memory[at]=static_cast<std::uint8_t>(value);}
 void equal(){
  check(sameFrame(frame(*ref),frame(*native)),"IRQ full frame/flags/clocks/history/indent mismatch");
  check(a.count==b.count,"IRQ live IO count mismatch");
  for(unsigned n=0;n<a.count;++n){const auto&x=a.log[n];const auto&y=b.log[n];check(x.clock==y.clock&&x.pending==y.pending&&x.high==y.high&&x.pc==y.pc&&x.sp==y.sp&&x.r0==y.r0&&x.address==y.address&&x.depth==y.depth&&x.ccr==y.ccr&&x.exr==y.exr&&x.value==y.value&&x.write==y.write,"IRQ live IO context/order/time mismatch");}
  for(auto r:Regions)check(std::memcmp(ref->memory+r.begin,native->memory+r.begin,r.size)==0,"IRQ touched memory/canary mismatch");
 }
 kstation::NativeIrq::Result step(int expectNative=-1){
  a.count=b.count=0;const auto before=frame(*native);
  const auto result=kstation::NativeIrq::step(*native);
  if(expectNative>=0)check(result.stepped==bool(expectNative),"Unexpected native IRQ admission");
  if(!result.stepped){check(sameFrame(before,frame(*native))&&b.count==0,"Rejected IRQ task mutated state/IO");native->step();++metrics.fallbacks;}
  else{++metrics.nativeSteps;metrics.admissions+=result.entered;metrics.returns+=result.returned;}
  ref->step();equal();++metrics.steps;metrics.io+=b.count;
  for(unsigned n=0;n<b.count;++n)if(!b.log[n].write&&b.log[n].address==0xfffff2)++metrics.syscrReads;
  return result;
 }
 void guardOnly(){a.count=b.count=0;const auto before=frame(*native);const auto result=kstation::NativeIrq::step(*native);check(!result.stepped&&sameFrame(before,frame(*native))&&b.count==0,"IRQ guard modified state");++metrics.guards;}
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
 if(argc<2){std::puts("SKIP: decoded private K-Station OS required");return 77;}
 std::ifstream input(argv[1],std::ios::binary);std::vector<std::uint8_t>rom{std::istreambuf_iterator<char>(input),{}};check(rom.size()==49728,"Wrong K firmware size");
 check(rom[0x98]==0&&rom[0x99]==0&&rom[0x9a]==9&&rom[0x9b]==0x2c&&rom[0xd0]==0&&rom[0xd1]==0&&rom[0xd2]==0xa&&rom[0xd3]==0x50,"Wrong K IRQ vectors");
 Fixture f(rom);watch=true;
 for(std::uint32_t pc=0;pc<0x10000;++pc){const bool body=(pc>=0x92c&&pc<0x946)||(pc>=0xa50&&pc<0xa64);check(kstation::NativeIrq::candidate(pc,0)==body&&kstation::NativeIrq::candidate(pc,1),"IRQ cheap candidate mismatch");}
 // All CCR values, signed byte/word boundaries and distinct bus/stack regions.
 for(auto pc:Phases)for(unsigned flags=0;flags<256;++flags)for(auto word:Words)for(unsigned ue:{0u,8u})for(auto sp:{Stacks[0],Stacks[3],Stacks[5]}){
  f.prepare(pc,flags,word,randomValue(),sp,ue);const auto r=f.step(1);check(!r.entered&&r.returned==(pc==0x944||pc==0xa62),"IRQ boundary lifecycle mismatch");
 }
 const auto boundaryCases=f.metrics.cases;
 // Admission accepts the lowest pending vector and executes its first PUSH in
 // the SAME original step, preserving old-PC/old-CCR IRQ stack observations.
 for(unsigned vector:{38u,52u})for(unsigned flags=0;flags<128;++flags)for(unsigned ue:{0u,8u})for(auto word:Words)for(auto sp:Stacks)for(unsigned tie:{0u,1u}){
  const auto pending=(std::uint64_t(1)<<vector)|(tie?(std::uint64_t(1)<<60):0);
  f.prepare((flags&1)?0x3ae9:0x3ae8,flags,word,randomValue(),sp,ue,pending);const auto r=f.step(1);
  check(r.entered&&!r.returned&&r.kind==(vector==38?kstation::NativeIrq::Kind::Timer3:kstation::NativeIrq::Kind::SerialError),"IRQ admission result mismatch");
 }
 const auto admissionCases=f.metrics.cases-boundaryCases;
 // Masked pending reads SYSCR even when the current handler phase does not
 // otherwise access IO. Unsupported unmasked vectors must reject unchanged.
 for(auto pc:Phases)for(unsigned flags:{0u,0x80u,0xffu})for(unsigned vector:{0u,28u,38u,52u,53u,60u})for(unsigned ue:{0u,8u}){
  f.prepare(pc,flags,0x7fff,0xc7,Stacks[0],ue,std::uint64_t(1)<<vector);
  const bool supported=(flags&0x80)||vector==38||vector==52;f.step(supported?1:0);
 }
 for(unsigned pc:{0x3ae8u,0x92du,0x938u,0xa51u,0xa5cu})for(unsigned flags:{0u,0x80u}){
  f.prepare(pc,flags,0,0,Stacks[0],0);f.guardOnly();
  f.native->pending_irqs_high=1;f.guardOnly();f.native->pending_irqs_high=0;f.native->interruptMode2=true;f.guardOnly();
 }
 for(auto pc:Phases){f.prepare(pc,0,0,0,Stacks[0],0);f.native->pending_irqs_high=1;f.guardOnly();f.native->pending_irqs_high=0;f.native->interruptMode2=true;f.guardOnly();}
 const auto guardCases=f.metrics.cases-boundaryCases-admissionCases;
 // Complete handlers: no manual caller bypass, and pending IRQ injection at a
 // real intermediate boundary. Each original instruction remains an oracle.
 for(unsigned vector:{38u,52u})for(unsigned flags=0;flags<128;++flags)for(unsigned ue:{0u,8u})for(auto word:{0u,0x7fffu,0x8000u,0xffffu})for(auto sp:Stacks){
  f.prepare(0x3ae8,flags,word,randomValue(),sp,ue,std::uint64_t(1)<<vector);
  bool returned=false;unsigned phases=0;
  while(!returned&&phases<12){const auto r=f.step(1);if(phases==2)f.ref->pending_irqs=f.native->pending_irqs=std::uint64_t(1)<<60;returned=r.returned;++phases;}
  check(returned&&phases==(vector==38?9u:8u),"Whole IRQ handler did not return");++f.metrics.whole;
 }
 for(unsigned outer:{38u,52u})for(unsigned flags:{0u,0x10u,0x2fu,0x7fu})for(unsigned ue:{0u,8u})for(auto sp:Stacks){
  f.prepare(0x3ae8,flags,0x7fff,0xff,sp,ue,std::uint64_t(1)<<outer);f.step(1);
  f.ref->ccr&=0x7f;f.native->ccr&=0x7f;const unsigned inner=outer==38?52:38;f.ref->pending_irqs=f.native->pending_irqs=std::uint64_t(1)<<inner;
  check(f.step(1).entered,"Nested IRQ not admitted");unsigned returned=0;
  for(unsigned n=0;n<24&&returned<2;++n)if(f.step(1).returned)++returned;
  check(returned==2,"Nested IRQs did not return");++f.metrics.nested;
 }
 // Both supported pending vectors: mode0 must service38 first, retain52
 // through the masked body and admit52 immediately after the first return.
 for(unsigned flags:{0u,0x10u,0x2fu,0x7fu})for(unsigned ue:{0u,8u})for(auto sp:Stacks){
  f.prepare(0x3ae8,flags,0x8000,0xff,sp,ue,(std::uint64_t(1)<<38)|(std::uint64_t(1)<<52));
  unsigned entered=0,returned=0;
  for(unsigned n=0;n<24&&returned<2;++n){const auto r=f.step(1);if(r.entered){check(r.kind==(entered==0?kstation::NativeIrq::Kind::Timer3:kstation::NativeIrq::Kind::SerialError),"Lowest pending IRQ order mismatch");++entered;}if(r.returned)++returned;}
  check(entered==2&&returned==2,"Supported pending tie lifecycle mismatch");++f.metrics.nested;
 }
 for(auto pc:Phases){f.prepare(pc,0,0,0,Stacks[0],0,(std::uint64_t(1)<<28)|(std::uint64_t(1)<<38)|(std::uint64_t(1)<<52));f.step(0);}
 for(unsigned vector:{38u,52u})for(unsigned budget:{0u,1u,2u,3u,4u,5u,7u,8u,16u,31u,64u,127u,128u,255u}){
  f.prepare(0x3ae8,0,0xffff,0xff,Stacks[0],8,std::uint64_t(1)<<vector);const auto begin=f.ref->cycles;
  while(f.ref->cycles-begin<budget)f.step();check(f.ref->cycles-begin==f.native->cycles-begin,"IRQ slice overshoot mismatch");++f.metrics.slices;
 }
 // Exact memory/clock contexts around uint64 guest-clock wrap.
 for(auto pc:Phases){f.prepare(pc,0xff,0x8000,0xff,Stacks[0],0,std::uint64_t(1)<<60);f.ref->cycles=f.native->cycles=~std::uint64_t(0)-4;f.step(1);}
 watch=false;check(allocations==0,"Native IRQ processing allocated");
 // Unexpected live target is an explicit failure AFTER real arbiter effects,
 // never a hidden fallback after SYSCR/stack/vector IO has already occurred.
 for(unsigned vector:{38u,52u}){
  f.prepare(0x3ae8,0x2f,0x8000,0,Stacks[0],0,std::uint64_t(1)<<vector);f.native->memory[vector*4+2]=1;f.native->memory[vector*4+3]=0x40;
  const auto old=frame(*f.native);bool threw=false;try{(void)kstation::NativeIrq::step(*f.native);}catch(const std::logic_error&){threw=true;}
  check(threw&&f.native->getSP()==static_cast<std::uint32_t>(old.regs[7])-4&&f.native->pending_irqs==0&&f.native->cycles!=old.cycles&&f.native->getPC()==old.pc&&f.native->irqDepth()==old.depth,"Unexpected IRQ target did not stop before first phase");++f.metrics.guards;
 }
 char report[2048];std::snprintf(report,sizeof(report),"{\"schema\":\"kstation.native-irq.v1\",\"passed\":true,\"boundary_cases\":%llu,\"admission_cases\":%llu,\"guard_cases\":%llu,\"total_cases\":%llu,\"oracle_steps\":%llu,\"native_steps\":%llu,\"native_admissions\":%llu,\"native_returns\":%llu,\"fallback_steps\":%llu,\"ordered_context_io\":%llu,\"syscr_reads\":%llu,\"guards\":%llu,\"whole_handlers\":%llu,\"nested_cases\":%llu,\"slices\":%llu,\"processing_new_allocations\":%llu}\n",static_cast<unsigned long long>(boundaryCases),static_cast<unsigned long long>(admissionCases),static_cast<unsigned long long>(guardCases),static_cast<unsigned long long>(f.metrics.cases),static_cast<unsigned long long>(f.metrics.steps),static_cast<unsigned long long>(f.metrics.nativeSteps),static_cast<unsigned long long>(f.metrics.admissions),static_cast<unsigned long long>(f.metrics.returns),static_cast<unsigned long long>(f.metrics.fallbacks),static_cast<unsigned long long>(f.metrics.io),static_cast<unsigned long long>(f.metrics.syscrReads),static_cast<unsigned long long>(f.metrics.guards),static_cast<unsigned long long>(f.metrics.whole),static_cast<unsigned long long>(f.metrics.nested),static_cast<unsigned long long>(f.metrics.slices),static_cast<unsigned long long>(allocations));
 std::fputs(report,stdout);if(argc>2){std::ofstream output(argv[2]);output<<report;check(bool(output),"Cannot save IRQ proof report");}return 0;
}catch(const std::exception&e){watch=false;std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
