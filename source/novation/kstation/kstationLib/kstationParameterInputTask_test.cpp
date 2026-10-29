#include "kstationNativePatchUnpack.h"
#include "kstationNativeParameterUpdate.h"
#include "kstationNativeParameterInput.h"
#include "kstationNativeIrq.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>
#include <cstring>
#include <new>
#include <cstdlib>

namespace {
bool watch=false;uint64_t allocations=0;
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
uint32_t rng=0x4b484449;uint32_t randomValue(){rng^=rng<<13;rng^=rng>>17;return rng^=rng<<5;}
class Original final:public H8SEmulator{public:int readHistory()const{return lastread;}int writeHistory()const{return lastwrite;}void histories(int r,int w){lastread=r;lastwrite=w;}int irqDepth()const{return indent;}void irqDepth(int n){indent=n;}};
struct Event {uint64_t cycles,pending,high;std::array<int32_t,8>regs;uint32_t pc,address;int depth;uint8_t flags,exr,value;bool write;};
template<class C>class Bus final:public H8SDevice {
public:
 C&c;explicit Bus(C&cpu):c(cpu){}std::array<Event,64>events{};unsigned count=0,reads=0,delay=0,group=0;
 void record(uint32_t a,uint8_t v,bool write){check(count<events.size(),"Parameter input test tape overflow");auto&e=events[count++];e.cycles=c.cycles;e.pending=c.pending_irqs;e.high=c.pending_irqs_high;for(unsigned i=0;i<8;++i)e.regs[i]=c.reg32(i);e.pc=uint32_t(c.getPC());e.address=a;e.depth=c.irqDepth();e.flags=c.ccr;e.exr=c.exr;e.value=v;e.write=write;}
 uint8_t read(uint32_t a)override{uint8_t v=c.memory[a];if(a==0x600004){++reads;v=reads<=delay?0:uint8_t(group==1?16:7);}record(a,v,false);return v;}
 void write(uint32_t a,uint8_t v)override{record(a,v,true);c.memory[a]=v;if(a==0x600000&&v==8){group=1;reads=0;}if(a==0x60000e){group=2;reads=0;}if(group==2&&a==0x600004){} }
};
struct Region{uint32_t at,size;};
constexpr Region Regions[]={{0,16},{0xba00,0x300},{0xbd00,0x300},{0x18000,0x1000},{0x19000,0x1000},{0xcd70,16},{0xcf00,0x400},{0xfffd00,0x300},{0xfff0,32},{0x1ffff0,32},{0x3ffff0,48},{0x600000,16}};
constexpr uint32_t Stacks[]{0xffff00,0xfffdc0,0x200001,0x3fffff,0x400001,0x1000001,0x100ffff};
struct Metrics{uint64_t graphs=0,phases=0,io=0,shortBudgets=0,faults=0,nested=0;};
class Fixture {
public:
 std::unique_ptr<Original>a=std::make_unique<Original>();std::unique_ptr<kstation::Stage1Cpu>b=std::make_unique<kstation::Stage1Cpu>();Bus<Original>x{*a};Bus<kstation::Stage1Cpu>y{*b};kstation::NativeParameterInput input;kstation::NativeParameterUpdate native;kstation::NativePatchUnpack unpack;Metrics m;uint64_t stubs=0;
 explicit Fixture(const std::vector<uint8_t>&rom){a->loadmem(rom.data(),unsigned(rom.size()),0);b->loadmem(rom.data(),unsigned(rom.size()),0);for(auto q:Regions){a->memmap(&x,q.at,q.size);b->memmap(&y,q.at,q.size);}for(unsigned at:{0x7930u,0x4358u,0x571eu}){byte(at,0x54);byte(at+1,0x70);}}
 void byte(uint32_t at,uint8_t v){a->memory[at&0xffffff]=b->memory[at&0xffffff]=v;}
 void word(uint32_t at,uint32_t v){byte(at,uint8_t(v>>8));byte(at+1,uint8_t(v));}void wide(uint32_t at,uint32_t v){word(at,v>>16);word(at+2,v);}
 void prepare(uint32_t entry,unsigned flags,uint32_t sp,unsigned offset,unsigned value,bool editor,bool match){input.reset();native.reset();unpack.reset();for(auto q:Regions)if(q.at!=0xbd00)for(unsigned i=0;i<q.size;++i)byte(q.at+i,uint8_t(randomValue()));for(unsigned i=0;i<8;++i)a->reg32(i)=b->reg32(i)=int32_t(randomValue());a->reg32(1)=b->reg32(1)=int32_t(offset);a->reg8(6)=b->reg8(6)=int8_t(value);byte(0xcd7f,editor);byte(0xcd81,uint8_t(match?offset:offset+1));byte(0xbb0e,match?1:0);a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->setSP(sp);b->setSP(sp);wide(sp,0x3ae8);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=uint8_t(randomValue());a->irqDepth(3);b->irqDepth(3);a->cycles=b->cycles=uint64_t(randomValue())*3;a->pending_irqs=b->pending_irqs=0;a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=false;a->histories(0x20000,0xfffe6e);b->histories(0x20000,0xfffe6e);x.count=y.count=0;++m.graphs;}
 void equal(){bool ok=a->cycles==b->cycles&&a->getPC()==b->getPC()&&a->ccr==b->ccr&&a->exr==b->exr&&a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->readHistory()==b->readHistory()&&a->writeHistory()==b->writeHistory()&&a->irqDepth()==b->irqDepth();for(unsigned i=0;i<8;++i)ok&=a->reg32(i)==b->reg32(i);if(!ok){watch=false;std::fprintf(stderr,"PC%x/%x clocks%llu/%llu flags%x/%x R0%x/%x R4%x/%x read%x/%x write%x/%x\n",a->getPC(),b->getPC(),a->cycles,b->cycles,a->ccr,b->ccr,unsigned(a->reg32(0)),unsigned(b->reg32(0)),unsigned(a->reg32(4)),unsigned(b->reg32(4)),a->readHistory(),b->readHistory(),a->writeHistory(),b->writeHistory());for(unsigned i=0;i<8;++i)std::fprintf(stderr,"ER%u %x/%x\n",i,unsigned(a->reg32(i)),unsigned(b->reg32(i)));throw std::runtime_error("Parameter input caller frame differs");}check(x.count==y.count,"Parameter input IO count differs");for(unsigned n=0;n<x.count;++n){const auto&p=x.events[n];const auto&q=y.events[n];if(!(p.cycles==q.cycles&&p.pending==q.pending&&p.high==q.high&&p.regs==q.regs&&p.pc==q.pc&&p.address==q.address&&p.depth==q.depth&&p.flags==q.flags&&p.exr==q.exr&&p.value==q.value&&p.write==q.write)){watch=false;std::fprintf(stderr,"IO%u PC%x/%x addr%x/%x flags%x/%x cyc%llu/%llu SP%x/%x value%x/%x depth%d/%d\n",n,p.pc,q.pc,p.address,q.address,p.flags,q.flags,p.cycles,q.cycles,unsigned(p.regs[7]),unsigned(q.regs[7]),p.value,q.value,p.depth,q.depth);throw std::runtime_error("Parameter input live IO context differs");}}}
 void step(){const auto at=a->getPC();x.count=y.count=0;bool stepped=input.step(*b);if(!stepped)stepped=native.step(*b);if(!stepped&&kstation::NativePatchUnpack::candidate(uint32_t(at)))stepped=unpack.step(*b);if(!stepped&&(at==0x7930||at==0x4358||at==0x571e)){b->step();stepped=true;++stubs;}check(stepped,"Parameter native phase rejected");a->step();equal();++m.phases;m.io+=y.count;}
 void whole(){for(unsigned n=0;n<4000&&a->getPC()!=0x3ae8;++n)step();check(a->getPC()==0x3ae8&&!native.active()&&!input.active(),"Unpack did not return");for(auto q:Regions)check(!std::memcmp(a->memory+q.at,b->memory+q.at,q.size),"Unpack RAM differs");}

};
}
void*operator new(std::size_t n){if(watch)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(int argc,char**argv){try{
 if(argc<2)return 77;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};check(rom.size()==49728,"Parameter input private ROM size");Fixture f(rom);watch=true;



 for(unsigned controller=0;controller<256;++controller)for(unsigned value=0;value<256;++value){f.prepare(0x57b8,0xff,Stacks[0],0,value,false,false);f.a->reg8(14)=f.b->reg8(14)=int8_t(controller);f.whole();}
 for(unsigned entry:{0x575au,0x5768u,0x576au})for(unsigned controller=0;controller<256;++controller)for(unsigned value:{0u,1u,63u,64u,127u,128u,255u}){f.prepare(entry,0xff,Stacks[0],0xca00,value,false,false);f.a->reg8(14)=f.b->reg8(14)=int8_t(controller);f.whole();}
 for(unsigned flags=0;flags<256;++flags)for(unsigned controller:{0u,50u,51u,103u,104u,105u,106u,107u,108u,255u}){f.prepare(0x575a,flags,Stacks[0],0xca00,64,true,true);f.a->reg8(14)=f.b->reg8(14)=int8_t(controller);f.whole();}
 for(auto sp:Stacks)for(unsigned entry:{0x575au,0x57b8u,0x5768u,0x576au}){f.prepare(entry,0xff,sp,0xca00,127,true,true);f.a->reg8(14)=f.b->reg8(14)=105;f.whole();}
 {using T=kstation::PackedControllerTask;T::Credits credits;unsigned reads=0;for(unsigned controller:{0u,104u,105u,106u,107u,255u})for(unsigned value:{0u,1u,63u,127u,255u}){T task(controller,value);unsigned transfers=0;while(!task.complete()){const auto cost=task.nextClocks(credits),before=reads;std::array<unsigned char,sizeof(task)>snapshot{};std::memcpy(snapshot.data(),&task,sizeof(task));auto read=[&](T::Read field)->uint32_t{++reads;switch(field){case T::Read::Descriptor:return 0x2000;case T::Read::Shift:return 2;case T::Read::Limit:return 128;case T::Read::Mask:return 3;case T::Read::Tag:return 0;case T::Read::LeftShift:return 3;case T::Read::Offset:return 6;case T::Read::Packed:return 0x55;}return 0;};for(unsigned budget=0;budget<cost;++budget){check(!task.advanceBefore(budget,credits,read)&&reads==before&&!std::memcmp(snapshot.data(),&task,sizeof(task)),"Packed CC short budget changed state/IO");++f.m.shortBudgets;}check(task.advanceBefore(cost,credits,read)==cost,"Packed CC admission");check(++transfers<2000,"Packed CC did not finish");}}
 }
 watch=false;check(!allocations,"Parameter input processing allocated");char out[512];std::snprintf(out,sizeof(out),"{\"passed\":true,\"graphs\":%llu,\"phases\":%llu,\"ordered_context_io\":%llu,\"explicit_stub_children\":%llu,\"short_budget_checks\":%llu,\"nested_irqs\":%llu,\"processing_allocations\":%llu}\n",f.m.graphs,f.m.phases,f.m.io,f.stubs,f.m.shortBudgets,f.m.nested,allocations);std::fputs(out,stdout);if(argc>2){std::ofstream save(argv[2]);save<<out;}return 0;
 }catch(const std::exception&e){watch=false;std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
