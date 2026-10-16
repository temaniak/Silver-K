// Original H8 emulator adapter. This translation unit is linked exclusively
// into comparison/test executables, never ksLib or a plugin target.
#include "ksControllerState.h"
#include "h8s/h8s.hpp"
#include "ksKeySearchCalls.h"
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
struct Install {Install(){setControllerReferenceFactoryForTests(&create);setKeySearchPartialForTests(&partialKeys);}} install;
}
}
