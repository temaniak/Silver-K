// Original H8 emulator adapter. This translation unit is linked exclusively
// into comparison/test executables, never ksLib or a plugin target.
#include "ksControllerState.h"
#include "h8s/h8s.hpp"
#include "ksKeySearchCalls.h"
#include "ksParameterScaleCalls.h"
#include "ksControllerJobs.h"
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
struct Install {Install(){setControllerReferenceFactoryForTests(&create);setKeySearchPartialForTests(&partialKeys);setParameterScalePartialForTests(&partialScale);setControlShiftPartialForTests(&partialShift);}} install;
}
}
