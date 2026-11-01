#include "kstationNativeKeyOwner.h"
#include "kstationNativeVoiceGlobal.h"
#include "kstationNativeVoiceGlobalOwner.h"
#include "kstationNativeVoiceAllocation.h"
#include "kstationNativeVoiceUpdate.h"
#include "kstationNativeDspTransfer.h"
#include "kstationNativeMidiSysExOwner.h"
#include "kstationNativeMidiEvents.h"
#include "kstationNativeVoiceAllocation.h"
#include "kstationNativeMidiEvent.h"
#include "kstationNativeVoiceUpdate.h"
#include "kstationNativeDspTransfer.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
class Original:public H8SEmulator {public:int rh()const{return lastread;}int wh()const{return lastwrite;}void histories(int a,int b){lastread=a;lastwrite=b;}int depth()const{return indent;}void depth(int n){indent=n;}};
uint32_t seed=0x2cf41c91;uint32_t rnd(){seed^=seed<<13;seed^=seed>>17;return seed^=seed<<5;}
struct Event {uint64_t time;uint32_t pc,sp,at,r0,r1,r6;uint8_t ccr,value;bool write;};
template<class C>struct Bus:H8SDevice {C&c;std::array<Event,128>events{};unsigned n=0,group=0;bool liveStatus=false;explicit Bus(C&x):c(x){}void record(uint32_t at,uint8_t v,bool w){check(n<events.size(),"bus overflow");events[n++]={c.cycles,uint32_t(c.getPC()),c.getSP(),at,uint32_t(c.reg32(0)),uint32_t(c.reg32(1)),uint32_t(c.reg32(6)),c.ccr,v,w};}uint8_t read(uint32_t at)override{auto v=at==0x600004&&!liveStatus?uint8_t(group==1?16:7):c.memory[at];record(at,v,false);return v;}void write(uint32_t at,uint8_t v)override{record(at,v,true);c.memory[at]=v;if(at==0x600000&&v==8)group=1;if(at==0x60000e)group=2;}};
struct F{std::unique_ptr<Original>a=std::make_unique<Original>();std::unique_ptr<kstation::Stage1Cpu>b=std::make_unique<kstation::Stage1Cpu>();Bus<Original>x{*a};Bus<kstation::Stage1Cpu>y{*b};uint64_t graphs=0,phases=0,events=0,children=0,handlerCalls=0,activityCalls=0;uint32_t stack=0xffff00;
 explicit F(const std::vector<uint8_t>&r){a->loadmem(r.data(),unsigned(r.size()),0);b->loadmem(r.data(),unsigned(r.size()),0);for(auto q:std::array<std::pair<uint32_t,uint32_t>,8>{{{0xbfa0,0x1000},{0xc9b6,0x200},{0xcbb6,0x24a},{0xd000,0x1400},{0xfffd20,0x210},{0xffff00,0x80},{0xffffd0,4},{0x600000,16}}}){a->memmap(&x,q.first,q.second);b->memmap(&y,q.first,q.second);}}
 void byte(uint32_t p,uint8_t v){a->memory[p&0xffffff]=b->memory[p&0xffffff]=v;}void word(uint32_t p,uint16_t v){byte(p,v>>8);byte(p+1,v);}void wide(uint32_t p,uint32_t v){word(p,v>>16);word(p+2,v);}
 void prep(uint32_t pc,uint8_t flags){for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rnd());a->pc=a->makepc(pc);b->pc=b->makepc(pc);a->setSP(stack);b->setSP(stack);wide(stack,0x7000);a->ccr=b->ccr=flags;a->exr=b->exr=uint8_t(rnd());a->cycles=b->cycles=rnd();a->histories(0x20000,0x30000);b->histories(0x20000,0x30000);a->depth(2);b->irqDepth(2);a->pending_irqs=b->pending_irqs=0;a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=false;x.n=y.n=0;x.group=y.group=0;}
 void equal(){bool ok=a->getPC()==b->getPC()&&a->cycles==b->cycles&&a->ccr==b->ccr&&a->exr==b->exr&&a->getSP()==b->getSP()&&a->rh()==b->readHistory()&&a->wh()==b->writeHistory()&&a->depth()==b->irqDepth();for(unsigned i=0;i<8;++i)ok&=a->regs[i].er==b->regs[i].er;if(!ok){std::fprintf(stderr,"PC=%x/%x cycles=%llu/%llu CCR=%x/%x SP=%x/%x R0=%x/%x R1=%x/%x R6=%x/%x history=%x/%x %x/%x\n",a->getPC(),b->getPC(),a->cycles,b->cycles,a->ccr,b->ccr,a->getSP(),b->getSP(),unsigned(a->reg32(0)),unsigned(b->reg32(0)),unsigned(a->reg32(1)),unsigned(b->reg32(1)),unsigned(a->reg32(6)),unsigned(b->reg32(6)),a->rh(),b->readHistory(),a->wh(),b->writeHistory());throw std::runtime_error("frame mismatch");}check(x.n==y.n,"bus count");for(unsigned i=0;i<x.n;++i){auto&p=x.events[i];auto&q=y.events[i];if(!(p.time==q.time&&p.pc==q.pc&&p.sp==q.sp&&p.at==q.at&&p.r0==q.r0&&p.r1==q.r1&&p.r6==q.r6&&p.ccr==q.ccr&&p.value==q.value&&p.write==q.write)){std::fprintf(stderr,"bus at %x/%x, pc %x/%x ccr %x/%x time %llu/%llu\n",p.at,q.at,p.pc,q.pc,p.ccr,q.ccr,p.time,q.time);throw std::runtime_error("bus differs");}}events+=x.n;x.n=y.n=0;for(auto q:std::array<std::pair<uint32_t,uint32_t>,3>{{{0xc9b6,0x200},{0xfffd20,0x210},{0xfffef0,0x20}}})check(!std::memcmp(a->memory+q.first,b->memory+q.first,q.second),"RAM mismatch");}
 void oracle(){a->step();b->step();equal();}
 void reset(uint8_t flags,uint8_t mode){prep(0x2f80,flags);byte(0xcd0e,mode);kstation::NativeMidiSysExReset t;unsigned n=0;while(!t.task().complete()){check(++n<10,"reset hang");check(t.step(*b),"reset phase");a->step();equal();check(!std::memcmp(a->memory+0xcc06,b->memory+0xcc06,8),"reset state mismatch");check(a->memory[0xcd0e]==b->memory[0xcd0e],"reset mode mismatch");++phases;}++graphs;}
 void key(uint8_t flags,uint16_t head,uint16_t tail,uint16_t event,uint8_t transpose,uint8_t local,uint8_t saved){prep(0x3b20,flags);word(0xc492,0);word(0xc494,0);word(0xc9b6,0);word(0xc9b8,0);byte(0xbb15,flags&1);byte(0xcd26,flags);byte(0xbb16,uint8_t(flags&15));byte(0xbb26,uint8_t((flags&2)?flags&15:(flags+1)&15));word(0xc6ac,head);word(0xc6aa,tail);word(0xc6ae + head,event);byte(0xcd78,transpose);byte(0xbb28,local);byte(0xcd86+uint8_t(event),saved);kstation::NativeKeyOwner owner;unsigned n=0,stubs=0;while(uint32_t(a->getPC())!=0x7000||owner.active()){if(++n>=1000){std::fprintf(stderr,"key hang pc=%x active=%d\n",unsigned(a->getPC()),owner.active());throw std::runtime_error("key hang");}if(owner.step(*b)){auto pc=uint32_t(a->getPC());if(pc==0x1e30||pc==0x261c||pc==0x2640||pc==0x26be||pc==0x27f4||pc==0xb0c)++children;a->step();equal();check(!std::memcmp(a->memory+0xc6aa,b->memory+0xc6aa,4),"key queue state");check(!std::memcmp(a->memory+0xcd86,b->memory+0xcd86,256),"key map state");++phases;continue;}equal();if(uint32_t(a->getPC())==0x7000&&!owner.active())break;check(false,"key missing native child");}++graphs;}
};
}
int main(int argc,char**argv){try{if(argc<2)return 77;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(in),{}};check(rom.size()==49728,"ROM size");F f(rom);for(unsigned flags=0;flags<256;++flags)for(uint16_t head:{uint16_t(0),uint16_t(2),uint16_t(510)})for(uint8_t key:{uint8_t(0),uint8_t(63),uint8_t(127),uint8_t(255)})for(uint8_t velocity:{uint8_t(0),uint8_t(1),uint8_t(127),uint8_t(128),uint8_t(255)})for(uint8_t local:{uint8_t(0),uint8_t(1)})f.key(uint8_t(flags),head,uint16_t(head^2),uint16_t((velocity<<8)|key),uint8_t(key^0x55),local,uint8_t(key^0x80));for(unsigned flags=0;flags<256;++flags)f.key(uint8_t(flags),510,510,0,0,0,0);std::printf("{\"passed\":true,\"graphs\":%llu,\"native_phases\":%llu,\"ordered_bus_events\":%llu,\"native_children\":%llu}\n",(unsigned long long)f.graphs,(unsigned long long)f.phases,(unsigned long long)f.events,(unsigned long long)f.children);return 0;}catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
