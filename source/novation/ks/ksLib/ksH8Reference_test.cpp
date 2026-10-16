// Original H8 emulator adapter. This translation unit is linked exclusively
// into comparison/test executables, never ksLib or a plugin target.
#include "ksControllerState.h"
#include "h8s/h8s.hpp"
#include "ksKeySearchCalls.h"
#include "ksParameterScaleCalls.h"
#include "ksControllerJobs.h"
#include "ksDspWordCalls.h"
#include "ksDspTransport.h"
#include "ksControllerBoundary.h"
#include <algorithm>
#include <memory>
namespace ks {
namespace {
class Original final:public ControllerReference,private H8SDevice {
    struct Core final:H8SEmulator {
        using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;
    };
    std::unique_ptr<Core> core=std::make_unique<Core>();
    ControllerCpu* shared=nullptr;
    void toCore(){
        core->pc=core->makepc(shared->getPC());
        for(unsigned i=0;i<8;++i)core->regs[i].er=shared->regs[i].er;
        core->ccr=shared->ccr;core->exr=shared->exr;core->cycles=shared->cycles;
        core->pending_irqs=shared->pending_irqs;core->pending_irqs_high=shared->pending_irqs_high;
        core->interruptMode2=shared->interruptMode2;
        std::copy(std::begin(shared->interruptPriority),std::end(shared->interruptPriority),std::begin(core->interruptPriority));
        core->lastread=shared->lastread;core->lastwrite=shared->lastwrite;core->indent=shared->indent;
    }
    void fromCore(){
        shared->pc=ControllerCpu::makepc(uint32_t(core->getPC()));
        for(unsigned i=0;i<8;++i)shared->regs[i].er=core->regs[i].er;
        shared->ccr=core->ccr;shared->exr=core->exr;shared->cycles=core->cycles;
        shared->pending_irqs=core->pending_irqs;shared->pending_irqs_high=core->pending_irqs_high;
        shared->interruptMode2=core->interruptMode2;
        std::copy(std::begin(core->interruptPriority),std::end(core->interruptPriority),std::begin(shared->interruptPriority));
        shared->lastread=core->lastread;shared->lastwrite=core->lastwrite;shared->indent=core->indent;
    }
    uint8_t read(uint32_t a)override{
        fromCore();const auto value=mappedRead(*shared,a);toCore();return value;
    }
    void write(uint32_t a,uint8_t v)override{
        fromCore();mappedWrite(*shared,a,v);toCore();
    }

public:
    Original(){core->memmap(this,0,1<<24);}
    void step(ControllerCpu& state)override{shared=&state;toCore();core->step();fromCore();shared=nullptr;}
};
std::unique_ptr<ControllerReference> create(){return std::make_unique<Original>();}
bool partialKeys(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x317a,2,f.move(r[4].rl,r[5].rl))
    N(0x317c,2,f.move(r[4].rh,r[4].rl))
    N(0x317e,2,r[4].rh&=0x7;value(h,r[4].rh))
    N(0x3180,2,r[4].rl&=0xfffffff8;value(h,r[4].rl))
    N(0x3182,2,f.move(r[1].rl,r[4].rl))
    N(0x3184,2,f.shift(r[1].rl,1,false))
    N(0x3186,2,f.shift(r[1].rl,1,false))
    N(0x3188,2,f.shift(r[1].rl,1,false))
    N(0x318a,2,r[1].r&=0xff;value(h,r[1].r))
    N(0x318c,2,r[1].er&=0xffff;value(h,r[1].er))
    N(0x318e,2,r[1].er=h.add32(r[1].er,r[3].er))
    N(0x3190,2,f.move(r[2].rl,h.read8(r[1].er)))
    N(0x3192,2,f.branch(bool(h.ccr&4),0x31a2))
    N(0x3194,2,f.inc(r[4].rh,1))
    N(0x3196,2,h.sub8(r[4].rh,0x8))
    N(0x3198,2,f.branch(bool(h.ccr&4),0x31a2))
    N(0x319a,2,flag(h,4,(uint8_t(r[2].rl)&(1u<<(uint8_t(r[4].rh)&7)))==0))
    N(0x319c,2,f.branch(bool(h.ccr&4),0x3194))
    N(0x319e,2,r[4].rl|=r[4].rh;value(h,r[4].rl))
    N(0x31a0,2,f.branch(true,0x31aa))
    N(0x31a2,2,r[1].er=int32_t(uint32_t(r[1].er)+0x1))
    N(0x31a4,2,f.move(r[4].rh,0xffffffff))
    N(0x31a6,2,r[4].rl=h.add8(r[4].rl,0x8))
    N(0x31a8,2,f.branch(!(h.ccr&8),0x3190))
    N(0x31aa,2,f.ret())
    N(0x31ac,2,f.move(r[4].rl,r[5].rl))
    N(0x31ae,2,f.move(r[4].rh,r[4].rl))
    N(0x31b0,2,r[4].rh&=0x7;value(h,r[4].rh))
    N(0x31b2,2,r[4].rl&=0xfffffff8;value(h,r[4].rl))
    N(0x31b4,2,f.move(r[1].rl,r[4].rl))
    N(0x31b6,2,f.shift(r[1].rl,1,false))
    N(0x31b8,2,f.shift(r[1].rl,1,false))
    N(0x31ba,2,f.shift(r[1].rl,1,false))
    N(0x31bc,2,r[1].r&=0xff;value(h,r[1].r))
    N(0x31be,2,r[1].er&=0xffff;value(h,r[1].er))
    N(0x31c0,2,r[1].er=h.add32(r[1].er,r[3].er))
    N(0x31c2,2,f.move(r[2].rl,h.read8(r[1].er)))
    N(0x31c4,2,f.branch(bool(h.ccr&4),0x31d2))
    N(0x31c6,2,f.inc(r[4].rh,-1))
    N(0x31c8,2,f.branch(bool(h.ccr&8),0x31d2))
    N(0x31ca,2,flag(h,4,(uint8_t(r[2].rl)&(1u<<(uint8_t(r[4].rh)&7)))==0))
    N(0x31cc,2,f.branch(bool(h.ccr&4),0x31c6))
    N(0x31ce,2,r[4].rl|=r[4].rh;value(h,r[4].rl))
    N(0x31d0,2,f.branch(true,0x31da))
    N(0x31d2,2,r[1].er=int32_t(uint32_t(r[1].er)-0x1))
    N(0x31d4,2,f.move(r[4].rh,0x8))
    N(0x31d6,2,r[4].rl=h.sub8(r[4].rl,r[4].rh))
    N(0x31d8,2,f.branch(!(h.ccr&8),0x31c2))
    N(0x31da,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialScale(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0xa69e,2,f.move(r[1].e,r[0].r))
    N(0xa6a0,2,f.branch(!(h.ccr&8),0xa6ac))
    N(0xa6a2,2,r[0].r=h.sub16(0,r[0].r))
    N(0xa6a4,4,h.sub16(r[0].r,0xffff8000))
    N(0xa6a8,2,f.branch(!(h.ccr&4),0xa6ac))
    N(0xa6aa,2,f.inc(r[0].r,-1))
    N(0xa6ac,2,f.move(r[1].rl,r[0].rh))
    N(0xa6ae,2,f.shift(r[1].rl,1,false))
    N(0xa6b0,2,f.shift(r[1].rl,1,false))
    N(0xa6b2,2,r[0].rh&=0x3;value(h,r[0].rh))
    N(0xa6b4,2,r[0].rh|=0x4;value(h,r[0].rh))
    N(0xa6b6,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0xa6b8,2,r[1].rl=h.add8(r[1].rl,0xfffffff5))
    N(0xa6ba,2,f.branch(bool(h.ccr&4),0xa6cc))
    N(0xa6bc,2,f.branch(bool(h.ccr&8),0xa6c6))
    N(0xa6be,2,f.shift(r[0].er,1,true))
    N(0xa6c0,2,f.inc(r[1].rl,-1))
    N(0xa6c2,2,f.branch(!(h.ccr&4),0xa6be))
    N(0xa6c4,2,f.branch(true,0xa6cc))
    N(0xa6c6,2,f.shift(r[0].er,1,false))
    N(0xa6c8,2,f.inc(r[1].rl,1))
    N(0xa6ca,2,f.branch(!(h.ccr&4),0xa6c6))
    N(0xa6cc,2,f.move(r[1].e,r[1].e))
    N(0xa6ce,2,f.branch(!(h.ccr&8),0xa6d2))
    N(0xa6d0,2,r[0].er=h.sub32(0,r[0].er))
    N(0xa6d2,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialWord(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x24d8,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x24e0,2,r[0].rl&=0x2;value(h,r[0].rl))
    N(0x24e2,2,f.branch(!(h.ccr&4),0x24fc))
    N(0x24e4,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0x24e6,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x24ee,2,r[0].rl&=0x2;value(h,r[0].rl))
    N(0x24f0,2,f.branch(!(h.ccr&4),0x24fc))
    N(0x24f2,2,f.inc(r[0].e,-1))
    N(0x24f4,2,f.branch(!(h.ccr&4),0x24e6))
    N(0x24f6,4,f.call(0x2304))
    N(0x24fa,2,f.branch(true,0x24e4))
    N(0x24fc,2,f.move(r[0].r,r[1].e))
    N(0x24fe,8,f.store8(r[5].er+10,r[0].rh))
    N(0x2506,8,f.store8(r[5].er+12,r[0].rl))
    N(0x250e,8,f.store8(r[5].er+14,r[1].rh))
    N(0x2516,2,f.inc(r[4].r,1))
    N(0x2518,6,f.store16(0x159f2,r[4].r))
    N(0x251e,2,f.ret())
    N(0x2520,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x2528,2,r[0].rl&=0x2;value(h,r[0].rl))
    N(0x252a,2,f.branch(!(h.ccr&4),0x2544))
    N(0x252c,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0x252e,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x2536,2,r[0].rl&=0x2;value(h,r[0].rl))
    N(0x2538,2,f.branch(!(h.ccr&4),0x2544))
    N(0x253a,2,f.inc(r[0].e,-1))
    N(0x253c,2,f.branch(!(h.ccr&4),0x252e))
    N(0x253e,4,f.call(0x2304))
    N(0x2542,2,f.branch(true,0x252c))
    N(0x2544,2,f.move(r[0].r,r[1].e))
    N(0x2546,8,f.store8(r[5].er+10,r[0].rh))
    N(0x254e,8,f.store8(r[5].er+12,r[0].rl))
    N(0x2556,8,f.store8(r[5].er+14,r[1].rh))
    N(0x255e,2,f.ret())
 default:return false;
 }
#undef N
}
ControllerJobs::Result partialShift(ControllerCpu& h,uint64_t deadline){
 using namespace controllerDetail;ControllerJobs::Result result;const unsigned entry=unsigned(h.getPC());
        auto& r=h.regs;const bool left=entry==0xa6be;
        const uint8_t initial=uint8_t(r[1].rl);
        unsigned remaining=left?initial:uint8_t(0-initial);if(!remaining)remaining=256;
        const unsigned count=unsigned(std::min<uint64_t>(remaining,(deadline-h.cycles)/9));
        if(!count)return result;
        const uint32_t original=uint32_t(r[0].er);
        const uint32_t shifted=count>=32?0:left?original<<count:original>>count;
        const bool carry=count>32?false:bool(original&(1u<<(left?32-count:count-1)));
        r[0].er=int32_t(shifted);flag(h,1,carry);
        const int delta=left?-1:1;
        const int old=int8_t(uint8_t(initial+delta*int(count-1))),last=old+delta;
        r[1].rl=int8_t(last);value(h,last);
        flag(h,2,(left?(uint32_t(old)&~uint32_t(last)):(~uint32_t(old)&uint32_t(last)))&0x80);
        h.cycles+=uint64_t(count)*9;h.lastread=int(entry+5);
        h.pc=h.makepc((h.ccr&4)?entry+6:entry);result.steps=count*3;
        return result;
}
// Historical batch ABI only, for fabricated partial-frame comparison tests.
unsigned partialWordBatch(ControllerCpu& h,uint64_t deadline){
 using namespace controllerDetail;
 const unsigned pc=unsigned(h.getPC());auto& r=h.regs;
 auto timing=[&](unsigned location,unsigned bytes){h.cycles+=3*(bytes/2);h.lastread=int(location+bytes-1);h.pc=h.makepc(location+1);};
 if(pc==0x24fc || pc==0x2544){
    if(deadline-h.cycles<48)return 0;
    timing(pc,2);r[0].r=r[1].e;
    auto put=[&](unsigned location,unsigned offset,int8_t v){timing(location,8);h.write8(v,r[5].er+offset);value(h,v);};
    put(pc+2,10,r[0].rh);put(pc+10,12,r[0].rl);put(pc+18,14,r[1].rh);
    h.pc=h.makepc(pc+26);return 4;
 }
 if(deadline-h.cycles<21)return 0;
 const unsigned ready=pc<0x2520?0x24fc:0x2544;
 timing(pc,8);r[0].rl=h.read8(r[5].er+4);
 timing(pc+8,2);r[0].rl&=2;value(h,r[0].rl);
 timing(pc+10,2);h.pc=h.makepc((h.ccr&4)?pc+12:ready);
 if(!(h.ccr&4))return 3;
 const bool retry=pc==0x24e6 || pc==0x252e;
 if(!retry){
    if(deadline-h.cycles<3)return 3;
    timing(pc+12,2);r[0].e=h.sub16(r[0].e,r[0].e);h.pc=h.makepc(pc+14);return 4;
 }
 if(deadline-h.cycles<6)return 3;
 timing(pc+12,2);const int old=r[0].e,remaining=old-1;r[0].e=int16_t(remaining);value(h,remaining);
 flag(h,2,(uint32_t(old)&~uint32_t(remaining))&0x8000);
 timing(pc+14,2);h.pc=h.makepc((h.ccr&4)?pc+16:pc);return 5;
}
struct Install {Install(){setControllerReferenceFactoryForTests(&create);setKeySearchPartialForTests(&partialKeys);setParameterScalePartialForTests(&partialScale);setControlShiftPartialForTests(&partialShift);setDspWordPartialForTests(&partialWord);setDspWordPartialBatchForTests(&partialWordBatch);}} install;
}
}
