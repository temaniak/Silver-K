#include "kstationStartupTask.h"
#include "h8s/h8s.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
using T=kstation::StartupTask;
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
class Original final:public H8SEmulator {public:int rh()const{return lastread;}int wh()const{return lastwrite;}int depth()const{return indent;}void init(int r,int w,int d){lastread=r;lastwrite=w;indent=d;}};
// Test-only independent caller projection. No opcode dispatch is called here.
class Projection final:public h8state {public:int rh()const{return lastread;}int wh()const{return lastwrite;}int depth()const{return depth_;}void init(int r,int w,int d){lastread=r;lastwrite=w;depth_=d;}void fetch(unsigned n){cycles+=3*(n/2);lastread=getPC()+n-1;}void value(int v){ccr=uint8_t((ccr&~14)|(v<0?8:0)|(v==0?4:0));}void ret(){--depth_;clockI(2);pc=popPC();}private:int depth_=0;};
struct Event {uint64_t clocks;uint32_t pc,at;std::array<int32_t,8> regs;uint8_t ccr,exr,value;bool write;};
template<class C>class Bus final:public H8SDevice {public:C&c;std::array<Event,4> events{};unsigned count=0;explicit Bus(C&x):c(x){}void record(uint32_t a,uint8_t v,bool w){check(count<events.size(),"IO overflow");auto&e=events[count++];e={c.cycles,uint32_t(c.getPC()),a,{},c.ccr,c.exr,v,w};for(unsigned n=0;n<8;++n)e.regs[n]=c.regs[n].er;}uint8_t read(uint32_t a)override{auto v=c.memory[a];record(a,v,false);return v;}void write(uint32_t a,uint8_t v)override{record(a,v,true);c.memory[a]=v;}};
uint32_t seed=0x4b424f54;uint32_t randomWord(){seed^=seed<<13;seed^=seed>>17;return seed^=seed<<5;}
unsigned prospective(unsigned at,unsigned width,int history){unsigned cost=0;for(unsigned n=0;n<width;++n){unsigned a=(at+n)&0xffffff;if(a<0x400000){if((history&1)||a!=(unsigned(history)|1))cost+=3;history=int(a);}else if(a>=0xfffd10&&a<0xffffa0){if((history&1)||a!=(unsigned(history)|1))++cost;history=int(a);}else{cost+=a>=0xffffa0?1:3;history=-1;}}return cost;}
unsigned address(T::Publication p){switch(p.field){case T::Field::P6Data:return 0xffffcb;case T::Field::P6Direction:return 0xffffc9;case T::Field::P8Data:return 0xffffcf;case T::Field::P8Direction:return 0xffffcd;case T::Field::P9Data:return 0xffffd2;case T::Field::P9Direction:return 0xffffd0;case T::Field::PAData:return 0xffffd3;case T::Field::PADirection:return 0xffffd1;case T::Field::PBData:return 0xffffd6;case T::Field::PBDirection:return 0xffffd4;case T::Field::P4Direction:return 0xffffc5;case T::Field::AccessState:return 0xffffed;case T::Field::WaitControl:return 0xffffee;case T::Field::InternalClear:return 0xfffd10+p.index*2;case T::Field::MainScratch:return 0xcd6c+p.index;}return 0;}
void project(Projection&b,T&task,unsigned cost,unsigned writeCost,unsigned retCost){auto st=task.stage();auto kind=task.kind();unsigned pc=b.getPC();unsigned bytes=st==T::Stage::BindTarget||st==T::Stage::BindRuntimeStack||st==T::Stage::BindCount&&kind==T::Kind::ColdClear?6:st==T::Stage::BindCount?4:2;
 b.fetch(bytes);b.pc=b.makepc(pc+1);
 check(task.advanceBefore(cost,writeCost,retCost,[&](T::Publication p){if(kind==T::Kind::Pins||kind==T::Kind::WaitStates)b.value(int8_t(p.value));if(p.width==2)b.write16(0,address(p));else b.write8(p.value,address(p));if(kind==T::Kind::ColdClear||kind==T::Kind::MainScratchClear)b.value(0);})==cost,"Phase refused admitted budget");
 unsigned next=pc+bytes;
 switch(st){case T::Stage::BindTarget:if(kind==T::Kind::ColdClear){b.setSP(0xfffd10);b.value(b.getSP());}else{b.reg32(0)=0xcd6c;b.value(b.reg32(0));}break;
 case T::Stage::BindCount:if(kind==T::Kind::ColdClear){b.reg32(0)=256;b.value(256);}else{b.reg16(9)=22;b.value(22);}break;
 case T::Stage::PrepareValue:b.reg8(kind==T::Kind::MainScratchClear?9:8)=task.publication().value;b.value(int8_t(task.publication().value));break;
 case T::Stage::Publish:break;
 case T::Stage::AdvanceTarget:if(kind==T::Kind::ColdClear)b.setSP(b.getSP()+2);else{++b.reg32(0);b.value(b.reg32(0));}break;
 case T::Stage::Decrement:{auto&r=b.reg16(kind==T::Kind::ColdClear?0:9);int old=r,v=old-1;b.value(v);if((old&~v)&0x8000)b.ccr|=2;r=int16_t(v);break;}
 case T::Stage::Repeat:if(task.remaining())next=kind==T::Kind::ColdClear?0x14e:0x3a64;break;
 case T::Stage::BindRuntimeStack:b.setSP(0xffff10);b.value(b.getSP());break;
 case T::Stage::Return:b.ret();return;
 case T::Stage::Complete:break;}
 b.pc=b.makepc(next);
}
}
int main(int argc,char**argv){try{if(argc<2){std::puts("SKIP: private K OS2.1.01 required");return 77;}std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(f),{}};check(rom.size()==49728,"ROM size");auto a=std::make_unique<Original>();auto b=std::make_unique<Projection>();a->loadmem(rom.data(),rom.size(),0);b->loadmem(rom.data(),rom.size(),0);Bus<Original>x(*a);Bus<Projection>y(*b);for(auto range:{std::pair{0xffffc0,64},std::pair{0xfffd00,0x210},std::pair{0xcd60,64}}){a->memmap(&x,range.first,range.second);b->memmap(&y,range.first,range.second);}
 uint64_t cases=0,phases=0,io=0,shorts=0;constexpr unsigned stacks[]={0xffff00,0xfffd80,0x200001,0x400001,0x1000001};
 for(unsigned k=0;k<4;++k)for(unsigned flags=0;flags<256;++flags)for(unsigned trial=0;trial<(k<2?5u:1u);++trial){T task{T::Kind(k)};for(unsigned n=0;n<8;++n)a->regs[n].er=b->regs[n].er=randomWord();for(auto range:{std::pair{0xffffc0,64},std::pair{0xfffd00,0x210},std::pair{0xcd60,64}})for(unsigned n=0;n<unsigned(range.second);++n)a->memory[range.first+n]=b->memory[range.first+n]=uint8_t(randomWord());a->setSP(stacks[trial]);b->setSP(stacks[trial]);for(unsigned n=0;n<4;++n)a->memory[(stacks[trial]+n)&0xffffff]=b->memory[(stacks[trial]+n)&0xffffff]=uint8_t(0x003ae8u>>((3-n)*8));a->cycles=b->cycles=flags==255?~uint64_t(0)-8:randomWord();a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=uint8_t(randomWord());a->pending_irqs=b->pending_irqs=0;a->pending_irqs_high=b->pending_irqs_high=0;a->init(0x1234,flags&1?0xcd6b:0xfffd0f,2);b->init(a->rh(),a->wh(),2);unsigned entry=k==0?0x280:k==1?0x2f4:k==2?0x142:0x3a58;a->pc=a->makepc(entry);b->pc=b->makepc(entry);
 while(!task.complete()){x.count=y.count=0;auto pub=task.publication();unsigned wc=task.stage()==T::Stage::Publish?prospective(address(pub),pub.width,b->wh()):0;unsigned rc=5+prospective(b->getSP(),4,int(b->getPC()+1));unsigned cost=task.nextClocks(wc,rc);auto before=task;for(unsigned budget=0;budget<cost;++budget){check(task.advanceBefore(budget,wc,rc,[&](auto){throw std::runtime_error("Short budget IO");})==0,"Short budget accepted");check(task.stage()==before.stage()&&task.index()==before.index()&&task.remaining()==before.remaining(),"Short budget mutated");++shorts;}unsigned pc=a->getPC();uint64_t clocks=b->cycles;project(*b,task,cost,wc,rc);a->step();bool same=a->getPC()==b->getPC()&&a->cycles==b->cycles&&a->ccr==b->ccr&&a->exr==b->exr&&a->rh()==b->rh()&&a->wh()==b->wh()&&a->depth()==b->depth();for(unsigned n=0;n<8;++n)same&=a->regs[n].er==b->regs[n].er;if(!same||b->cycles-clocks!=cost){std::fprintf(stderr,"kind%u PC%x clocks%llu/%llu cost%u CCR%x/%x read%x/%x write%x/%x\n",k,pc,a->cycles,b->cycles,cost,a->ccr,b->ccr,a->rh(),b->rh(),a->wh(),b->wh());throw std::runtime_error("Original context/clocks differs");}check(x.count==y.count,"IO count differs");for(unsigned n=0;n<x.count;++n){auto&p=x.events[n];auto&q=y.events[n];check(p.clocks==q.clocks&&p.pc==q.pc&&p.at==q.at&&p.regs==q.regs&&p.ccr==q.ccr&&p.exr==q.exr&&p.value==q.value&&p.write==q.write,"Live IO context differs");}io+=x.count;++phases;}
 for(auto range:{std::pair{0xffffc0,64},std::pair{0xfffd00,0x210},std::pair{0xcd60,64}})check(std::memcmp(a->memory+range.first,b->memory+range.first,range.second)==0,"RAM/canary differs");check(task.advanceBefore(100,0,7,[&](auto){throw std::runtime_error("Complete IO");})==0,"Complete advanced");++cases;}
 std::printf("{\"passed\":true,\"cases\":%llu,\"phases\":%llu,\"ordered_context_io\":%llu,\"short_budgets\":%llu,\"production_decoder_calls\":0,\"integrated_full_boot\":false}\n",cases,phases,io,shorts);return 0;}catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
