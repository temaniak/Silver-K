// Original H8 emulator adapter. This translation unit is linked exclusively
// into comparison/test executables, never ksLib or a plugin target.
#include "ksControllerState.h"
#include "h8s/h8s.hpp"
#include "ksKeySearchCalls.h"
#include "ksParameterScaleCalls.h"
#include "ksControllerJobs.h"
#include "ksDspWordCalls.h"
#include "ksDspAddressCalls.h"
#include "ksDspFaultCalls.h"
#include "ksDspBootStreamCalls.h"
#include "ksDspBootSetupCalls.h"
#include "ksDspStartupCalls.h"
#include "ksDspRoutingCalls.h"
#include "ksPartControlCalls.h"
#include "ksVelocityCurveCalls.h"
#include "ksVoiceSelectionCalls.h"
#include "ksVoicePopulationCalls.h"
#include "ksKeyResetCalls.h"
#include "ksVoiceReleaseCalls.h"
#include "ksUnisonCalls.h"
#include "ksVoiceTrackingCalls.h"
#include "ksVelocityModulationCalls.h"
#include "ksGlideCalls.h"
#include "ksControllerScheduling.h"
#include "ksDspTransport.h"
#include "ksControllerBoundary.h"
#include "ksRuntimeBoundary.h"
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
bool partialAddress(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x2416,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x241e,2,r[0].rl&=0x4;value(h,r[0].rl))
    N(0x2420,2,f.branch(!(h.ccr&4),0x243a))
    N(0x2422,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0x2424,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x242c,2,r[0].rl&=0x4;value(h,r[0].rl))
    N(0x242e,2,f.branch(!(h.ccr&4),0x243a))
    N(0x2430,2,f.inc(r[0].e,-1))
    N(0x2432,2,f.branch(!(h.ccr&4),0x2424))
    N(0x2434,4,f.call(0x2304))
    N(0x2438,2,f.branch(true,0x2422))
    N(0x243a,2,f.move(r[0].rl,0x8))
    N(0x243c,8,f.store8(r[5].er+0,r[0].rl))
    N(0x2444,2,f.move(r[0].rl,0x0))
    N(0x2446,8,f.store8(r[5].er+10,r[0].rl))
    N(0x244e,8,f.store8(r[5].er+12,r[4].rh))
    N(0x2456,8,f.store8(r[5].er+14,r[4].rl))
    N(0x245e,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x2466,2,r[0].rl&=0x4;value(h,r[0].rl))
    N(0x2468,2,f.branch(!(h.ccr&4),0x2482))
    N(0x246a,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0x246c,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x2474,2,r[0].rl&=0x4;value(h,r[0].rl))
    N(0x2476,2,f.branch(!(h.ccr&4),0x2482))
    N(0x2478,2,f.inc(r[0].e,-1))
    N(0x247a,2,f.branch(!(h.ccr&4),0x246c))
    N(0x247c,4,f.call(0x2304))
    N(0x2480,2,f.branch(true,0x246a))
    N(0x2482,2,f.move(r[0].rl,0x0))
    N(0x2484,8,f.store8(r[5].er+0,r[0].rl))
    N(0x248c,2,f.ret())
    N(0x2560,6,f.move(r[5].er,0x600000))
    N(0x2566,2,f.branch(true,0x256e))
    N(0x2568,6,f.move(r[5].er,0x400000))
    N(0x256e,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x2576,2,r[0].rl&=0x4;value(h,r[0].rl))
    N(0x2578,2,f.branch(!(h.ccr&4),0x2592))
    N(0x257a,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0x257c,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x2584,2,r[0].rl&=0x4;value(h,r[0].rl))
    N(0x2586,2,f.branch(!(h.ccr&4),0x2592))
    N(0x2588,2,f.inc(r[0].e,-1))
    N(0x258a,2,f.branch(!(h.ccr&4),0x257c))
    N(0x258c,4,f.call(0x2304))
    N(0x2590,2,f.branch(true,0x257a))
    N(0x2592,2,f.move(r[0].rl,0x8))
    N(0x2594,8,f.store8(r[5].er+0,r[0].rl))
    N(0x259c,2,f.move(r[0].rl,0xffffff80))
    N(0x259e,8,f.store8(r[5].er+10,r[0].rl))
    N(0x25a6,8,f.store8(r[5].er+12,r[4].rh))
    N(0x25ae,8,f.store8(r[5].er+14,r[4].rl))
    N(0x25b6,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x25be,2,r[0].rl&=0x1;value(h,r[0].rl))
    N(0x25c0,2,f.branch(!(h.ccr&4),0x25da))
    N(0x25c2,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0x25c4,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x25cc,2,r[0].rl&=0x1;value(h,r[0].rl))
    N(0x25ce,2,f.branch(!(h.ccr&4),0x25da))
    N(0x25d0,2,f.inc(r[0].e,-1))
    N(0x25d2,2,f.branch(!(h.ccr&4),0x25c4))
    N(0x25d4,4,f.call(0x2304))
    N(0x25d8,2,f.branch(true,0x25c2))
    N(0x25da,2,f.move(r[0].rl,0x0))
    N(0x25dc,8,f.store8(r[5].er+0,r[0].rl))
    N(0x25e4,8,f.move(r[4].rh,h.read8(r[5].er+10)))
    N(0x25ec,8,f.move(r[4].rl,h.read8(r[5].er+12)))
    N(0x25f4,8,f.move(r[0].rl,h.read8(r[5].er+14)))
    N(0x25fc,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialFault(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x660,2,f.push(r[6].r))
    N(0x662,6,f.move(r[6].rl,h.read8(0xffec22)))
    N(0x668,2,r[6].rl^=0x7;value(h,r[6].rl))
    N(0x66a,6,f.store8(0xffec22,r[6].rl))
    N(0x670,2,f.store8(0xffff61,r[6].rl))
    N(0x672,2,f.move(r[6].rl,0x36))
    N(0x674,2,f.store8(0xffff62,r[6].rl))
    N(0x676,4,h.write8(uint8_t(h.read8(0xffff62))&~(1u<<0x5),0xffff62))
    N(0x67a,2,f.push(r[0].r))
    N(0x67c,4,f.move(r[0].r,0x3))
    N(0x680,2,f.inc(r[0].r,-1))
    N(0x682,2,f.branch(!(h.ccr&4),0x680))
    N(0x684,2,f.pop(r[0].r))
    N(0x686,4,h.write8(uint8_t(h.read8(0xffff62))|(1u<<0x5),0xffff62))
    N(0x68a,2,f.pop(r[6].r))
    N(0x68c,2,f.ret())
    N(0x2304,4,f.call(0x660))
    N(0x2308,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialBootStream(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x23ce,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x23d6,2,r[0].rl&=0x2;value(h,r[0].rl))
    N(0x23d8,2,f.branch(!(h.ccr&4),0x23f2))
    N(0x23da,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0x23dc,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x23e4,2,r[0].rl&=0x2;value(h,r[0].rl))
    N(0x23e6,2,f.branch(!(h.ccr&4),0x23f2))
    N(0x23e8,2,f.inc(r[0].e,-1))
    N(0x23ea,2,f.branch(!(h.ccr&4),0x23dc))
    N(0x23ec,4,f.call(0x2304))
    N(0x23f0,2,f.branch(true,0x23da))
    N(0x23f2,2,f.move(r[0].rl,h.read8(r[4].er));r[4].er+=1;h.clockI(2))
    N(0x23f4,8,f.store8(r[5].er+10,r[0].rl))
    N(0x23fc,2,f.move(r[0].rl,h.read8(r[4].er));r[4].er+=1;h.clockI(2))
    N(0x23fe,8,f.store8(r[5].er+12,r[0].rl))
    N(0x2406,2,f.move(r[0].rl,h.read8(r[4].er));r[4].er+=1;h.clockI(2))
    N(0x2408,8,f.store8(r[5].er+14,r[0].rl))
    N(0x2410,2,f.inc(r[3].e,-1))
    N(0x2412,2,f.branch(!(h.ccr&4),0x23ce))
    N(0x2414,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialBootSetup(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{RuntimeBoundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x2332,4,f.move(r[0].rh,h.read8(r[4].er+1)))
    N(0x2336,4,f.move(r[0].rl,h.read8(r[4].er+2)))
    N(0x233a,2,f.move(r[3].e,r[0].r))
    N(0x233c,2,f.inc(r[3].e,2))
    N(0x233e,4,f.call(0x23ce))
    N(0x2342,2,f.push(r[0].r))
    N(0x2344,4,f.move(r[0].r,0xa))
    N(0x2348,2,f.push(r[0].r))
    N(0x234a,4,f.move(r[0].r,0xa67))
    N(0x234e,2,f.inc(r[0].r,-1))
    N(0x2350,2,f.branch(!(h.ccr&4),0x234e))
    N(0x2352,2,f.pop(r[0].r))
    N(0x2354,2,f.inc(r[0].r,-1))
    N(0x2356,2,f.branch(!(h.ccr&4),0x2348))
    N(0x2358,2,f.pop(r[0].r))
    N(0x235a,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x2362,2,r[0].rl&=0x10;value(h,r[0].rl))
    N(0x2364,2,f.branch(bool(h.ccr&4),0x237e))
    N(0x2366,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0x2368,8,f.move(r[0].rl,h.read8(r[5].er+4)))
    N(0x2370,2,r[0].rl&=0x10;value(h,r[0].rl))
    N(0x2372,2,f.branch(bool(h.ccr&4),0x237e))
    N(0x2374,2,f.inc(r[0].e,-1))
    N(0x2376,2,f.branch(!(h.ccr&4),0x2368))
    N(0x2378,4,f.call(0x2304))
    N(0x237c,2,f.branch(true,0x2366))
    N(0x237e,4,f.move(r[4].r,0x8600))
    N(0x2382,4,f.call(0x2416))
    N(0x2386,6,f.move(r[1].er,0x40000000))
    N(0x238c,4,f.move(r[2].e,0xb191))
    N(0x2390,4,f.move(r[6].e,0x200))
    N(0x2394,4,f.call(0x2520))
    N(0x2398,2,f.move(r[3].r,r[2].e))
    N(0x239a,2,r[3].er=uint16_t(r[3].er)*uint32_t(uint16_t(r[1].e));h.clockI(20))
    N(0x239c,2,f.move(r[0].rl,0x9))
    N(0x239e,2,f.shift(r[3].er,1,false))
    N(0x23a0,2,f.inc(r[0].rl,-1))
    N(0x23a2,2,f.branch(!(h.ccr&4),0x239e))
    N(0x23a4,2,r[1].er=h.add32(r[1].er,r[3].er))
    N(0x23a6,2,f.inc(r[6].e,-1))
    N(0x23a8,2,f.branch(!(h.ccr&4),0x2394))
    N(0x23aa,4,f.move(r[2].r,0x200))
    N(0x23ae,4,f.move(r[2].e,0x200))
    N(0x23b2,6,f.move(r[3].er,0x800000))
    N(0x23b8,2,f.divideUnsigned(r[2].r,r[3].er))
    N(0x23ba,2,f.move(r[1].e,r[3].r))
    N(0x23bc,2,r[3].r=h.sub16(r[3].r,r[3].r))
    N(0x23be,2,f.divideUnsigned(r[2].r,r[3].er))
    N(0x23c0,2,f.move(r[1].r,r[3].r))
    N(0x23c2,4,f.call(0x2520))
    N(0x23c6,2,f.inc(r[2].r,1))
    N(0x23c8,2,f.inc(r[2].e,-1))
    N(0x23ca,2,f.branch(!(h.ccr&4),0x23b2))
    N(0x23cc,2,f.ret())
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
namespace historicalTransport {
void timing(ControllerCpu& h,unsigned pc,unsigned bytes) {
    h.cycles += 3 * (bytes / 2);
    h.lastread = int(pc + bytes - 1);
    h.pc = h.makepc(pc + 1); // Original in-flight PC seen by the port recorder.
}
void portWrite(ControllerCpu& h,unsigned pc,unsigned offset,int8_t v) {
    timing(h,pc,8);h.write8(v,h.regs[5].er+offset);
    controllerDetail::value(h,v);
}
unsigned servicePhase(ControllerCpu& h,uint64_t deadline) {
    if(deadline<=h.cycles)return 0;
    auto& r=h.regs;const unsigned pc=h.getPC();
    const uint64_t available=deadline-h.cycles;
    unsigned mask=0,ready=0;
    switch(pc) {
    case 0x2416:case 0x2424:mask=4;ready=0x243a;break;
    case 0x245e:case 0x246c:mask=4;ready=0x2482;break;
    case 0x24d8:case 0x24e6:mask=2;ready=0x24fc;break;
    case 0x2520:case 0x252e:mask=2;ready=0x2544;break;
    case 0x256e:case 0x257c:mask=4;ready=0x2592;break;
    case 0x25b6:case 0x25c4:mask=1;ready=0x25da;break;
    default:break;
    }
    if(mask) {
        // One readiness observation, with the original mask/branch boundary.
        // Busy responses retain the live timeout path; never cache readiness.
        if(available<21)return 0;
        timing(h,pc,8);r[0].rl=h.read8(r[5].er+4);
        timing(h,pc+8,2);r[0].rl&=mask;controllerDetail::value(h,r[0].rl);
        timing(h,pc+10,2);h.pc=h.makepc((h.ccr&4)?pc+12:ready);
        if(!(h.ccr&4))return 3;
        const bool retry=pc==0x2424 || pc==0x246c || pc==0x24e6
            || pc==0x252e || pc==0x257c || pc==0x25c4;
        if(!retry) {
            // Start the live timeout only when the reference saw busy.
            if(deadline-h.cycles<3)return 3;
            timing(h,pc+12,2);r[0].e=h.sub16(r[0].e,r[0].e);
            h.pc=h.makepc(pc+14);return 4;
        }
        if(deadline-h.cycles<6)return 3;
        // Busy wait is a timed retry of the actual port observation. Preserve
        // the reference countdown/overflow flags and timeout continuation;
        // this is not a guessed completion time or a cached DSP response.
        timing(h,pc+12,2);
        const int old=r[0].e,remaining=old-1;r[0].e=int16_t(remaining);
        controllerDetail::value(h,remaining);
        controllerDetail::flag(h,2,(uint32_t(old)&~uint32_t(remaining))&0x8000);
        timing(h,pc+14,2);h.pc=h.makepc((h.ccr&4)?pc+16:pc);
        return 5;
    }
    switch(pc) {
    case 0x243a:case 0x2592: {
        // Address request: HF3, command byte, high/low address bytes.
        if(available<66)return 0;
        timing(h,pc,2);r[0].rl=8;
        portWrite(h,pc+2,0,r[0].rl);
        timing(h,pc+10,2);r[0].rl=pc==0x243a?0:int8_t(0x80);
        portWrite(h,pc+12,10,r[0].rl);
        portWrite(h,pc+20,12,r[4].rh);
        portWrite(h,pc+28,14,r[4].rl);
        h.pc=h.makepc(pc+36);return 6;
    }
    case 0x2482:case 0x25da:
        if(available<18)return 0;
        timing(h,pc,2);r[0].rl=0;
        portWrite(h,pc+2,0,0);h.pc=h.makepc(pc+10);return 2;
    case 0x24fc:case 0x2544:
        // Transfer one live 24-bit word. Address-cache maintenance and return
        // remain outside this job, preserving the public compatibility frame.
        if(available<48)return 0;
        timing(h,pc,2);r[0].r=r[1].e;
        portWrite(h,pc+2,10,r[0].rh);
        portWrite(h,pc+10,12,r[0].rl);
        portWrite(h,pc+18,14,r[1].rh);
        h.pc=h.makepc(pc+26);return 4;
    case 0x25e4:
        if(available<45)return 0;
        timing(h,pc,8);r[4].rh=h.read8(r[5].er+10);
        timing(h,pc+8,8);r[4].rl=h.read8(r[5].er+12);
        timing(h,pc+16,8);r[0].rl=h.read8(r[5].er+14);
        controllerDetail::value(h,r[0].rl);h.pc=h.makepc(pc+24);return 3;
    default:return 0;
    }
}
unsigned service(ControllerCpu& h,uint64_t deadline) {
    if(!h.nativeFetchClocksOnly || !h.nativeLowBase || !h.nativeLowWritable
        || deadline<=h.cycles
        || (h.regs[5].er!=0x400000 && h.regs[5].er!=0x600000))return 0;
    if(h.pending_irqs || h.pending_irqs_high) {
        // The periodic voice/HDI handler can have lower-priority UART work
        // pending. Preserve it, but do not confuse a masked IRQ with an event
        // that can preempt this job. These jobs never change EXR or IRQ masks.
        if(!h.interruptMode2)return 0;
        const unsigned mask=h.exr&7;
        for(unsigned base:{0u,64u}) {
            auto bits=base?h.pending_irqs_high:h.pending_irqs;
            while(bits) {
#if defined(__clang__) || defined(__GNUC__)
                const unsigned bit=unsigned(__builtin_ctzll(bits));
#else
                auto word=bits;unsigned bit=0;while(!(word&1)){++bit;word>>=1;}
#endif
                if(h.interruptPriority[base+bit]>mask)return 0;
                bits&=bits-1;
            }
        }
    }
    unsigned boundaries=0;
    // Continue the live request/response pipeline until the next scheduled
    // event or compatibility operation. No DSP or timer advances between
    // these original boundaries, and every port observation remains live.
    while(const unsigned n=servicePhase(h,deadline))boundaries+=n;
    return boundaries;
}

}
bool partialStartup(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{RuntimeBoundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x136e,2,f.push(r[6].r))
    N(0x1370,2,f.move(r[6].rh,h.read8(0xffff6f)))
    N(0x1372,2,r[6].rh&=~(1u<<0x3))
    N(0x1374,2,f.store8(0xffff6f,r[6].rh))
    N(0x1376,2,f.push(r[0].r))
    N(0x1378,4,f.move(r[0].r,0xa))
    N(0x137c,2,f.push(r[0].r))
    N(0x137e,4,f.move(r[0].r,0xa67))
    N(0x1382,2,f.inc(r[0].r,-1))
    N(0x1384,2,f.branch(!(h.ccr&4),0x1382))
    N(0x1386,2,f.pop(r[0].r))
    N(0x1388,2,f.inc(r[0].r,-1))
    N(0x138a,2,f.branch(!(h.ccr&4),0x137c))
    N(0x138c,2,f.pop(r[0].r))
    N(0x138e,2,r[6].rh|=(1u<<0x3))
    N(0x1390,2,f.store8(0xffff6f,r[6].rh))
    N(0x1392,2,f.push(r[0].r))
    N(0x1394,4,f.move(r[0].r,0xa))
    N(0x1398,2,f.push(r[0].r))
    N(0x139a,4,f.move(r[0].r,0xa67))
    N(0x139e,2,f.inc(r[0].r,-1))
    N(0x13a0,2,f.branch(!(h.ccr&4),0x139e))
    N(0x13a2,2,f.pop(r[0].r))
    N(0x13a4,2,f.inc(r[0].r,-1))
    N(0x13a6,2,f.branch(!(h.ccr&4),0x1398))
    N(0x13a8,2,f.pop(r[0].r))
    N(0x13aa,2,f.pop(r[6].r))
    N(0x13ac,2,f.ret())
    N(0x230c,4,f.call(0x136e))
    N(0x2310,6,f.move(r[5].er,0x400000))
    N(0x2316,6,f.move(r[4].er,0xbad6))
    N(0x231c,2,f.call(0x2332))
    N(0x231e,6,f.move(r[5].er,0x600000))
    N(0x2324,6,f.move(r[4].er,0xd0c3))
    N(0x232a,2,f.call(0x2332))
    N(0x232c,4,f.call(0x24a0))
    N(0x2330,2,f.ret())
    N(0x248e,6,f.move(r[5].er,0x600000))
    N(0x2494,4,f.move(r[0].r,0xffff))
    N(0x2498,6,f.store16(0x159f2,r[0].r))
    N(0x249e,2,f.ret())
    N(0x24a0,6,f.move(r[5].er,0x400000))
    N(0x24a6,4,f.move(r[0].r,0xffff))
    N(0x24aa,6,f.store16(0x159f2,r[0].r))
    N(0x24b0,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialRouting(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x24b2,6,f.store16(0x159f2,r[4].r))
    N(0x24b8,4,f.call(0x2416))
    N(0x24bc,2,f.ret())
    N(0x24be,6,f.move(r[5].er,0x600000))
    N(0x24c4,2,f.branch(true,0x24cc))
    N(0x24c6,6,f.move(r[5].er,0x400000))
    N(0x24cc,6,f.move(r[0].r,h.read16(0x159f2)))
    N(0x24d2,2,h.sub16(r[4].r,r[0].r))
    N(0x24d4,4,f.call(0x2416))
    N(0x77dc,6,f.move(r[0].r,h.read16(0x174a8)))
    N(0x77e2,2,f.move(r[4].r,r[4].r))
    N(0x77e4,2,f.branch(!(h.ccr&8),0x77ec))
    N(0x77e6,6,f.move(r[0].r,h.read16(0x174aa)))
    N(0x77ec,2,r[4].r=h.add16(r[4].r,r[0].r))
    N(0x77ee,4,f.call(0x24b2))
    N(0x77f2,2,f.ret())
    N(0x77f4,6,f.move(r[1].rl,h.read8(0x174a7)))
    N(0x77fa,2,r[1].r&=0xff;value(h,r[1].r))
    N(0x77fc,4,f.move(r[0].r,0x65))
    N(0x7800,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[1].r));h.clockI(20))
    N(0x7802,6,f.store16(0x174a8,r[0].r))
    N(0x7808,4,f.move(r[0].r,0x16))
    N(0x780c,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[1].r));h.clockI(20))
    N(0x780e,6,f.store16(0x174aa,r[0].r))
    N(0x7814,4,f.call(0x248e))
    N(0x7818,2,f.ret())
    N(0x781a,6,f.move(r[1].rl,h.read8(0x17526)))
    N(0x7820,6,f.move(r[0].rl,h.read8(0x167ba)))
    N(0x7826,2,r[1].rl=h.sub8(r[1].rl,r[0].rl))
    N(0x7828,2,f.branch(!(h.ccr&8),0x782c))
    N(0x782a,2,f.move(r[1].rl,0x0))
    N(0x782c,2,r[1].r&=0xff;value(h,r[1].r))
    N(0x782e,4,f.move(r[0].r,0x35))
    N(0x7832,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[1].r));h.clockI(20))
    N(0x7834,6,f.store16(0x174a8,r[0].r))
    N(0x783a,4,f.move(r[0].r,0x5))
    N(0x783e,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[1].r));h.clockI(20))
    N(0x7840,6,f.store16(0x174aa,r[0].r))
    N(0x7846,4,f.call(0x24a0))
    N(0x784a,2,f.ret())
    N(0x784c,6,f.move(r[0].r,h.read16(0x174aa)))
    N(0x7852,2,r[4].r=h.add16(r[4].r,r[0].r))
    N(0x7854,4,f.call(0x2560))
    N(0x7858,2,f.ret())
    N(0x785a,6,f.move(r[0].r,h.read16(0x174a8)))
    N(0x7860,2,f.move(r[4].r,r[4].r))
    N(0x7862,2,f.branch(!(h.ccr&8),0x786a))
    N(0x7864,6,f.move(r[0].r,h.read16(0x174aa)))
    N(0x786a,2,r[4].r=h.add16(r[4].r,r[0].r))
    N(0x786c,4,f.call(0x24cc))
    N(0x7870,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialPartControl(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x4000,6,f.move(r[0].rl,h.read8(0x17526)))
    N(0x4006,2,r[0].r&=0xff;value(h,r[0].r))
    N(0x4008,4,f.move(r[0].e,0x14e))
    N(0x400c,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20))
    N(0x400e,6,r[0].er=h.add32(r[0].er,0x160d2))
    N(0x4014,2,f.ret())
    N(0xa656,6,f.move(r[3].rl,h.read8(0x174a7)))
    N(0xa65c,2,f.move(r[3].rh,0x14))
    N(0xa65e,2,r[3].r=uint8_t(r[3].r)*uint32_t(uint8_t(r[3].rh));h.clockI(12))
    N(0xa660,2,r[3].er&=0xffff;value(h,r[3].er))
    N(0xa662,6,r[3].er=h.add32(r[3].er,0x17560))
    N(0xa668,2,f.ret())
    N(0xa66a,6,f.move(r[0].rl,h.read8(0x17526)))
    N(0xa670,2,f.move(r[0].rh,0x14))
    N(0xa672,2,r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12))
    N(0xa674,2,r[0].er&=0xffff;value(h,r[0].er))
    N(0xa676,6,r[0].er=h.add32(r[0].er,0x174ae))
    N(0xa67c,2,f.ret())
    N(0xa67e,2,r[0].r&=0xff;value(h,r[0].r))
    N(0xa680,4,f.move(r[2].e,0x14))
    N(0xa684,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[2].e));h.clockI(20))
    N(0xa686,6,r[0].er=h.add32(r[0].er,0x174ae))
    N(0xa68c,6,f.move(r[1].er,0x17512))
    N(0xa692,2,f.move(r[2].rl,h.read8(r[0].er));r[0].er+=1;h.clockI(2))
    N(0xa694,2,f.store8(r[1].er,r[2].rl))
    N(0xa696,2,f.inc(r[1].er,1))
    N(0xa698,2,f.inc(r[2].e,-1))
    N(0xa69a,2,f.branch(!(h.ccr&4),0xa692))
    N(0xa69c,2,f.ret())
 default:return false;
 }
#undef N
}
namespace historicalControl {

namespace {
using Result=ControllerJobs::Result;
using namespace controllerDetail;
// Low code addresses are aligned 16-bit fetches. Data reads/return-stack
// accesses remain actual, including their in-flight PC and paired bus clocks.
void timing(ControllerCpu& h,unsigned pc,unsigned bytes){
    h.cycles+=3*(bytes/2);h.lastread=int(pc+bytes-1);h.pc=h.makepc(pc+1);
}
unsigned returnCost(const ControllerCpu& h){
    const auto sp=h.getSP()&0xffffff;
    // Return reads must stay in actual RAM, with no timer/MMIO observation.
    if(sp>=0xffdc00 && sp<=0xfffbfc)return 17;
    if(sp>=0xa00000 && sp<=0xbffffc)return 17;
    return 0;
}
unsigned partPhase(ControllerCpu& h,uint64_t deadline){
    const unsigned pc=h.getPC();auto& r=h.regs;const auto available=deadline-h.cycles;
    switch(pc){
    case 0x4000:case 0xa66a:
        if(available<12)return 0;
        timing(h,pc,6);r[0].rl=h.read8(0x17526);value(h,r[0].rl);h.pc=h.makepc(pc+6);return 1;
    case 0x4006:
        if(available<3)return 0;
        timing(h,pc,2);r[0].r&=0xff;value(h,r[0].r);h.pc=h.makepc(0x4008);return 1;
    case 0x4008:
        if(available<6)return 0;
        timing(h,pc,4);r[0].e=0x14e;value(h,r[0].e);h.pc=h.makepc(0x400c);return 1;
    case 0x400c:
        if(available<23)return 0;
        timing(h,pc,2);r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20);
        h.pc=h.makepc(0x400e);return 1;
    case 0x400e:case 0xa676:
        if(available<9)return 0;
        timing(h,pc,6);r[0].er=h.add32(r[0].er,pc==0x400e?0x160d2:0x174ae);
        h.pc=h.makepc(pc+6);return 1;
    case 0x4014:case 0xa67c:{
        const unsigned cost=returnCost(h);if(!cost||available<cost)return 0;
        timing(h,pc,2);--h.indent;h.clockI(2);h.pc=h.popPC();return 1;
    }
    case 0xa670:
        if(available<3)return 0;
        timing(h,pc,2);r[0].rh=0x14;value(h,r[0].rh);h.pc=h.makepc(0xa672);return 1;
    case 0xa672:
        if(available<15)return 0;
        timing(h,pc,2);r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12);
        h.pc=h.makepc(0xa674);return 1;
    case 0xa674:
        if(available<3)return 0;
        timing(h,pc,2);r[0].er&=0xffff;value(h,r[0].er);h.pc=h.makepc(0xa676);return 1;
    default:return 0;
    }
}
}
ControllerJobs::Result service(ControllerCpu& h,uint64_t deadline){
    Result result;
    if(!h.nativeFetchClocksOnly||!h.nativeLowBase||!h.nativeLowWritable
        ||deadline<=h.cycles||!canDeferInterrupts(h))return result;
    const unsigned entry=h.getPC();
    if((entry>=0x4000 && entry<=0x4014)||(entry>=0xa66a && entry<=0xa67c)){
        unsigned bodyCost=0,steps=0;
        switch(entry){
        case 0x4000:bodyCost=53;steps=6;break;
        case 0x4006:bodyCost=41;steps=5;break;
        case 0x4008:bodyCost=38;steps=4;break;
        case 0x400c:bodyCost=32;steps=3;break;
        case 0x400e:bodyCost=9;steps=2;break;
        case 0x4014:steps=1;break;
        case 0xa66a:bodyCost=42;steps=6;break;
        case 0xa670:bodyCost=30;steps=5;break;
        case 0xa672:bodyCost=27;steps=4;break;
        case 0xa674:bodyCost=12;steps=3;break;
        case 0xa676:bodyCost=9;steps=2;break;
        case 0xa67c:steps=1;break;
        default:break;
        }
        const unsigned stackCost=returnCost(h);
        if(steps && stackCost && deadline-h.cycles>=bodyCost+stackCost){
            // Lift the whole remaining function when no event can observe
            // intermediate arithmetic. Retain every admitted SRAM/stack read
            // at its original cycle/PC, and preserve arbitrary partial frames.
            auto& r=h.regs;unsigned selected=uint8_t(r[0].rl),readCost=0;
            if(entry==0x4000||entry==0xa66a){
                timing(h,entry,6);selected=uint8_t(h.read8(0x17526));readCost=12;
            }
            if(steps>1){
                uint32_t offset=0,base=entry<0x5000?0x160d2:0x174ae;bool generalFlags=false;
                switch(entry){
                case 0x4000:case 0x4006:offset=selected*0x14e;break;
                case 0x4008:offset=uint16_t(r[0].r)*0x14eu;break;
                case 0x400c:offset=uint16_t(r[0].r)*uint32_t(uint16_t(r[0].e));generalFlags=true;break;
                case 0x400e:case 0xa676:offset=uint32_t(r[0].er);generalFlags=true;break;
                case 0xa66a:case 0xa670:offset=selected*0x14;break;
                case 0xa672:offset=uint8_t(r[0].rl)*uint32_t(uint8_t(r[0].rh));break;
                case 0xa674:offset=uint16_t(r[0].r);break;
                default:break;
                }
                if(generalFlags)r[0].er=h.add32(int32_t(offset),base);
                else {
                    r[0].er=int32_t(offset+base);
                    // Bounded formulas are nonzero, positive and below bit
                    // 27: C/V/Z/N/H clear, while I/UI/U are retained.
                    h.ccr&=0xd0;
                }
            }
            h.cycles+=bodyCost-readCost;
            const unsigned returnPc=entry<0x5000?0x4014:0xa67c;
            timing(h,returnPc,2);--h.indent;h.clockI(2);h.pc=h.popPC();
            result.steps=result.noteSteps=steps;return result;
        }
    }
    if(entry==0xa6be||entry==0xa6c6)return partialShift(h,deadline);
    while(h.cycles<deadline){
        const unsigned pc=h.getPC();
        if(!((pc>=0x4000 && pc<=0x4014)||(pc>=0xa66a && pc<=0xa67c)))break;
        const unsigned n=partPhase(h,deadline);if(!n)break;
        result.steps+=n;result.noteSteps+=n;
        // Return is the end of this job, even when a caller happens to return
        // into another job. Preserve the same explicit lifecycle boundary.
        if(pc==0x4014||pc==0xa67c)break;
    }
    return result;
}

}
bool partialVelocityCurve(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0xa5fc,2,f.move(r[0].rl,r[1].rh))
    N(0xa5fe,2,f.inc(r[2].rh,-1))
    N(0xa600,2,f.branch(bool(h.ccr&4),0xa60a))
    N(0xa602,2,f.move(r[1].rl,r[1].rh))
    N(0xa604,2,r[1].r=uint8_t(r[1].r)*uint32_t(uint8_t(r[0].rl));h.clockI(12))
    N(0xa606,2,f.shift(r[1].r,1,true))
    N(0xa608,2,f.branch(true,0xa5fe))
    N(0xa60a,2,f.move(r[1].rl,r[1].rh))
    N(0xa60c,2,r[1].rl^=0x7f;value(h,r[1].rl))
    N(0xa60e,2,f.move(r[2].rl,r[2].rl))
    N(0xa610,2,f.branch(!(h.ccr&4),0xa614))
    N(0xa612,2,f.ret())
    N(0xa614,2,f.inc(r[2].rl,-1))
    N(0xa616,2,f.branch(!(h.ccr&4),0xa61c))
    N(0xa618,2,f.move(r[1].rh,r[1].rl))
    N(0xa61a,2,f.ret())
    N(0xa61c,2,f.inc(r[2].rl,-1))
    N(0xa61e,2,f.branch(!(h.ccr&4),0xa628))
    N(0xa620,2,f.move(r[1].rl,r[1].rh))
    N(0xa622,2,r[1].r=uint8_t(r[1].r)*uint32_t(uint8_t(r[1].rh));h.clockI(12))
    N(0xa624,2,f.shift(r[1].r,1,true))
    N(0xa626,2,f.ret())
    N(0xa628,2,f.inc(r[2].rl,-1))
    N(0xa62a,2,f.branch(!(h.ccr&4),0xa632))
    N(0xa62c,2,r[1].r=uint8_t(r[1].r)*uint32_t(uint8_t(r[1].rh));h.clockI(12))
    N(0xa62e,2,f.shift(r[1].r,1,true))
    N(0xa630,2,f.ret())
    N(0xa632,2,f.inc(r[2].rl,-1))
    N(0xa634,2,r[2].r&=0xff;value(h,r[2].r))
    N(0xa636,2,r[2].er&=0xffff;value(h,r[2].er))
    N(0xa638,6,r[2].er=h.add32(r[2].er,0x1534e))
    N(0xa63e,2,f.move(r[2].rl,h.read8(r[2].er)))
    N(0xa640,2,flag(h,4,(uint8_t(r[2].rl)&(1u<<0x0))==0))
    N(0xa642,2,f.branch(!(h.ccr&4),0xa64a))
    N(0xa644,2,h.sub8(r[0].rl,r[2].rl))
    N(0xa646,2,f.branch(!(h.ccr&5),0xa650))
    N(0xa648,2,f.ret())
    N(0xa64a,2,h.sub8(r[0].rl,r[2].rl))
    N(0xa64c,2,f.branch(bool(h.ccr&1),0xa650))
    N(0xa64e,2,f.ret())
    N(0xa650,2,f.move(r[1].rh,0xffffff80))
    N(0xa652,2,f.ret())
    N(0xa654,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialVoiceSelection(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x8fc0,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0x8fc2,4,f.move(r[4].r,0x8085))
    N(0x8fc6,4,f.call(0x785a))
    N(0x8fca,2,f.ret())
    N(0xa338,4,f.call(0xa66a))
    N(0xa33c,8,f.move(r[4].rl,h.read8(r[6].er+0)))
    N(0xa344,2,f.shift(r[4].rl,2,false))
    N(0xa346,2,f.shift(r[4].rl,1,false))
    N(0xa348,2,r[4].rl&=0x3;value(h,r[4].rl))
    N(0xa34a,8,f.move(r[4].rh,h.read8(r[0].er+17)))
    N(0xa352,8,f.move(r[5].rh,h.read8(r[0].er+18)))
    N(0xa35a,6,f.move(r[5].rl,h.read8(0x17526)))
    N(0xa360,2,f.move(r[5].e,r[5].r))
    N(0xa362,2,r[2].rh=h.sub8(r[2].rh,r[2].rh))
    N(0xa364,6,f.store8(0x174a7,r[2].rh))
    N(0xa36a,6,f.move(r[2].rl,h.read8(0x174a6)))
    N(0xa370,6,f.move(r[1].er,0xffffffff))
    N(0xa376,2,h.sub8(r[4].rl,0x1))
    N(0xa378,4,f.branch(!(h.ccr&5),0xa44e))
    N(0xa37c,2,f.move(r[4].rh,r[4].rh))
    N(0xa37e,2,f.branch(bool(h.ccr&4),0xa386))
    N(0xa380,2,f.move(r[0].rl,0x1))
    N(0xa382,4,f.branch(true,0xa454))
    N(0xa386,2,f.move(r[0].rl,0x4))
    N(0xa388,6,f.store8(0x1752a,r[0].rl))
    N(0xa38e,2,f.move(r[4].rl,0x1))
    N(0xa390,4,f.call(0xa656))
    N(0xa394,10,f.move(r[0].er,h.read32(r[3].er+8)))
    N(0xa39e,8,f.move(r[5].rl,h.read8(r[3].er+4)))
    N(0xa3a6,8,f.move(r[5].rh,h.read8(r[3].er+12)))
    N(0xa3ae,2,h.sub16(r[5].r,r[5].e))
    N(0xa3b0,2,f.branch(bool(h.ccr&4),0xa3e6))
    N(0xa3b2,2,h.sub32(r[0].er,r[1].er))
    N(0xa3b4,2,f.branch(!(h.ccr&1),0xa3be))
    N(0xa3b6,2,f.move(r[1].er,r[0].er))
    N(0xa3b8,6,f.move(r[2].rh,h.read8(0x174a7)))
    N(0xa3be,6,f.move(r[0].rl,h.read8(0x174a7)))
    N(0xa3c4,2,f.inc(r[0].rl,1))
    N(0xa3c6,6,f.store8(0x174a7,r[0].rl))
    N(0xa3cc,2,f.inc(r[2].rl,-1))
    N(0xa3ce,2,f.branch(!(h.ccr&4),0xa390))
    N(0xa3d0,6,f.store8(0x174a7,r[2].rh))
    N(0xa3d6,6,f.store8(0x1752a,r[4].rl))
    N(0xa3dc,2,f.move(r[1].e,r[1].e))
    N(0xa3de,4,f.branch(!(h.ccr&4),0xa508))
    N(0xa3e2,4,f.branch(true,0xa524))
    N(0xa3e6,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0xa3e8,4,f.call(0xa656))
    N(0xa3ec,10,f.move(r[0].er,h.read32(r[3].er+8)))
    N(0xa3f6,8,f.move(r[5].rl,h.read8(r[3].er+4)))
    N(0xa3fe,8,f.move(r[5].rh,h.read8(r[3].er+12)))
    N(0xa406,2,h.sub16(r[5].r,r[5].e))
    N(0xa408,2,f.branch(!(h.ccr&4),0xa41e))
    N(0xa40a,2,f.move(r[5].r,r[0].e))
    N(0xa40c,2,r[5].rh&=0xffffffc0;value(h,r[5].rh))
    N(0xa40e,4,f.branch(!(h.ccr&4),0xa524))
    N(0xa412,2,h.sub32(r[0].er,r[1].er))
    N(0xa414,2,f.branch(bool(h.ccr&5),0xa41e))
    N(0xa416,2,f.move(r[1].er,r[0].er))
    N(0xa418,6,f.move(r[2].rh,h.read8(0x174a7)))
    N(0xa41e,6,f.move(r[0].rl,h.read8(0x174a7)))
    N(0xa424,2,f.inc(r[0].rl,1))
    N(0xa426,6,f.store8(0x174a7,r[0].rl))
    N(0xa42c,2,f.inc(r[2].rl,-1))
    N(0xa42e,2,f.branch(!(h.ccr&4),0xa3e8))
    N(0xa430,6,f.store8(0x174a7,r[2].rh))
    N(0xa436,2,f.move(r[1].e,r[1].e))
    N(0xa438,2,f.branch(bool(h.ccr&4),0xa444))
    N(0xa43a,2,f.move(r[4].rl,0x3))
    N(0xa43c,4,h.sub16(r[1].e,0x100))
    N(0xa440,2,f.branch(!(h.ccr&1),0xa444))
    N(0xa442,2,f.move(r[4].rl,0x2))
    N(0xa444,6,f.store8(0x1752a,r[4].rl))
    N(0xa44a,4,f.branch(true,0xa524))
    N(0xa44e,2,h.sub8(r[4].rl,0x2))
    N(0xa450,2,f.branch(!(h.ccr&4),0xa496))
    N(0xa452,2,f.move(r[0].rl,0x0))
    N(0xa454,6,f.store8(0x1752a,r[0].rl))
    N(0xa45a,4,f.call(0xa656))
    N(0xa45e,10,f.move(r[0].er,h.read32(r[3].er+8)))
    N(0xa468,2,f.move(r[0].e,r[0].e))
    N(0xa46a,4,f.branch(bool(h.ccr&4),0xa524))
    N(0xa46e,2,h.sub32(r[0].er,r[1].er))
    N(0xa470,2,f.branch(!(h.ccr&1),0xa47a))
    N(0xa472,2,f.move(r[1].er,r[0].er))
    N(0xa474,6,f.move(r[2].rh,h.read8(0x174a7)))
    N(0xa47a,6,f.move(r[0].rl,h.read8(0x174a7)))
    N(0xa480,2,f.inc(r[0].rl,1))
    N(0xa482,6,f.store8(0x174a7,r[0].rl))
    N(0xa488,2,f.inc(r[2].rl,-1))
    N(0xa48a,2,f.branch(!(h.ccr&4),0xa45a))
    N(0xa48c,6,f.store8(0x174a7,r[2].rh))
    N(0xa492,4,f.branch(true,0xa508))
    N(0xa496,6,f.move(r[4].rl,h.read8(0x17528)))
    N(0xa49c,2,f.move(r[0].rl,0x0))
    N(0xa49e,6,f.store8(0x1752a,r[0].rl))
    N(0xa4a4,4,f.call(0xa656))
    N(0xa4a8,10,f.move(r[0].er,h.read32(r[3].er+8)))
    N(0xa4b2,2,f.move(r[0].e,r[0].e))
    N(0xa4b4,2,f.branch(bool(h.ccr&4),0xa4e0))
    N(0xa4b6,8,f.move(r[5].rl,h.read8(r[3].er+4)))
    N(0xa4be,8,f.move(r[5].rh,h.read8(r[3].er+12)))
    N(0xa4c6,2,h.sub16(r[5].r,r[5].e))
    N(0xa4c8,2,f.branch(!(h.ccr&4),0xa4e0))
    N(0xa4ca,8,f.move(r[5].rl,h.read8(r[3].er+5)))
    N(0xa4d2,2,h.sub8(r[4].rl,r[5].rl))
    N(0xa4d4,2,f.branch(!(h.ccr&4),0xa4e0))
    N(0xa4d6,2,f.move(r[0].rl,0x4))
    N(0xa4d8,6,f.store8(0x1752a,r[0].rl))
    N(0xa4de,2,f.branch(true,0xa524))
    N(0xa4e0,2,h.sub32(r[0].er,r[1].er))
    N(0xa4e2,2,f.branch(!(h.ccr&1),0xa4ec))
    N(0xa4e4,2,f.move(r[1].er,r[0].er))
    N(0xa4e6,6,f.move(r[2].rh,h.read8(0x174a7)))
    N(0xa4ec,6,f.move(r[0].rl,h.read8(0x174a7)))
    N(0xa4f2,2,f.inc(r[0].rl,1))
    N(0xa4f4,6,f.store8(0x174a7,r[0].rl))
    N(0xa4fa,2,f.inc(r[2].rl,-1))
    N(0xa4fc,2,f.branch(!(h.ccr&4),0xa4a4))
    N(0xa4fe,6,f.store8(0x174a7,r[2].rh))
    N(0xa504,2,f.move(r[1].e,r[1].e))
    N(0xa506,2,f.branch(bool(h.ccr&4),0xa524))
    N(0xa508,4,f.call(0x77f4))
    N(0xa50c,4,f.move(r[1].e,0x2000))
    N(0xa510,4,f.move(r[4].r,0x72))
    N(0xa514,4,f.call(0x785a))
    N(0xa518,4,f.move(r[4].r,0x7a))
    N(0xa51c,4,f.call(0x785a))
    N(0xa520,4,f.call(0x8fc0))
 default:return false;
 }
#undef N
}
bool partialVoicePopulation(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0xa524,4,f.call(0xa656))
    N(0xa528,4,f.call(0xa66a))
    N(0xa52c,10,f.store32(r[3].er+0,r[6].er))
    N(0xa536,6,f.move(r[1].rl,h.read8(0x17528)))
    N(0xa53c,8,f.store8(r[3].er+5,r[1].rl))
    N(0xa544,8,f.move(r[2].rl,h.read8(r[0].er+14)))
    N(0xa54c,2,r[2].rl=h.add8(r[2].rl,0xffffffc0))
    N(0xa54e,2,r[1].rl=h.add8(r[1].rl,r[2].rl))
    N(0xa550,8,f.store8(r[3].er+13,r[1].rl))
    N(0xa558,2,r[1].rl=h.sub8(r[1].rl,r[1].rl))
    N(0xa55a,8,f.store8(r[0].er+17,r[1].rl))
    N(0xa562,8,f.move(r[1].rl,h.read8(r[0].er+18)))
    N(0xa56a,8,f.store8(r[3].er+12,r[1].rl))
    N(0xa572,6,f.move(r[1].rh,h.read8(0x17529)))
    N(0xa578,8,f.move(r[2].rl,h.read8(r[0].er+16)))
    N(0xa580,6,f.move(r[0].er,0x168fc))
    N(0xa586,8,f.move(r[2].rh,h.read8(r[0].er+245)))
    N(0xa58e,2,r[2].rh=h.add8(r[2].rh,0x2))
    N(0xa590,2,f.call(0xa5fc))
    N(0xa592,8,f.store8(r[3].er+6,r[1].rh))
    N(0xa59a,6,f.move(r[0].er,0x8000ffff))
    N(0xa5a0,10,f.store32(r[3].er+8,r[0].er))
    N(0xa5aa,6,f.move(r[0].rl,h.read8(0x17526)))
    N(0xa5b0,8,f.store8(r[3].er+4,r[0].rl))
    N(0xa5b8,4,f.call(0xa67e))
    N(0xa5bc,8,f.move(r[0].rl,h.read8(r[6].er+173)))
    N(0xa5c4,8,f.store8(r[3].er+18,r[0].rl))
    N(0xa5cc,4,f.call(0x77f4))
    N(0xa5d0,4,f.move(r[4].r,0x808c))
    N(0xa5d4,4,f.call(0x784c))
    N(0xa5d8,2,r[4].rh=h.add8(r[4].rh,0x3c))
    N(0xa5da,8,f.move(r[4].rl,h.read8(r[3].er+13)))
    N(0xa5e2,2,r[4].rl=h.sub8(r[4].rl,r[4].rh))
    N(0xa5e4,2,f.branch(!(h.ccr&1),0xa5e8))
    N(0xa5e6,2,r[4].rl=h.sub8(0,r[4].rl))
    N(0xa5e8,8,f.store8(r[3].er+7,r[4].rl))
    N(0xa5f0,2,f.move(r[0].rl,0x0))
    N(0xa5f2,8,f.store8(r[3].er+17,r[0].rl))
    N(0xa5fa,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialKeyReset(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x3074,4,f.push(r[0].er))
    N(0x3078,4,f.push(r[1].er))
    N(0x307c,2,f.push(r[2].r))
    N(0x307e,4,f.call(0x4000))
    N(0x3082,2,f.move(r[1].er,r[0].er))
    N(0x3084,6,r[1].er=h.add32(r[1].er,0xe0))
    N(0x308a,4,f.move(r[2].r,0x10))
    N(0x308e,2,r[1].er-=1;f.store8(r[1].er,r[2].rh);h.clockI(2))
    N(0x3090,2,f.inc(r[2].rl,-1))
    N(0x3092,2,f.branch(!(h.ccr&4),0x308e))
    N(0x3094,8,f.store8(r[0].er+307,r[2].rh))
    N(0x309c,8,f.store8(r[0].er+306,r[2].rh))
    N(0x30a4,8,f.store8(r[0].er+308,r[2].rh))
    N(0x30ac,2,f.pop(r[2].r))
    N(0x30ae,4,f.pop(r[1].er))
    N(0x30b2,4,f.pop(r[0].er))
    N(0x30b6,2,f.ret())
    N(0x30b8,4,f.push(r[0].er))
    N(0x30bc,4,f.push(r[1].er))
    N(0x30c0,2,f.push(r[2].r))
    N(0x30c2,4,f.call(0x4000))
    N(0x30c6,2,f.move(r[1].er,r[0].er))
    N(0x30c8,6,r[1].er=h.add32(r[1].er,0xd0))
    N(0x30ce,4,f.move(r[2].r,0x10))
    N(0x30d2,2,r[1].er-=1;f.store8(r[1].er,r[2].rh);h.clockI(2))
    N(0x30d4,2,f.inc(r[2].rl,-1))
    N(0x30d6,2,f.branch(!(h.ccr&4),0x30d2))
    N(0x30d8,2,f.pop(r[2].r))
    N(0x30da,4,f.pop(r[1].er))
    N(0x30de,4,f.pop(r[0].er))
    N(0x30e2,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialVoiceRelease(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x9374,2,r[2].r=h.sub16(r[2].r,r[2].r))
    N(0x9376,6,f.move(r[1].rl,h.read8(0x17526)))
    N(0x937c,2,f.move(r[0].rl,0x0))
    N(0x937e,6,f.store8(0x174a7,r[0].rl))
    N(0x9384,6,f.move(r[0].rl,h.read8(0x174a6)))
    N(0x938a,2,f.push(r[0].r))
    N(0x938c,2,f.push(r[1].r))
    N(0x938e,2,f.push(r[2].r))
    N(0x9390,4,f.call(0xa656))
    N(0x9394,8,f.move(r[2].r,h.read16(r[3].er+8)))
    N(0x939c,2,f.branch(bool(h.ccr&4),0x93e2))
    N(0x939e,8,f.move(r[0].rl,h.read8(r[3].er+4)))
    N(0x93a6,2,h.sub8(r[1].rl,r[0].rl))
    N(0x93a8,2,f.branch(!(h.ccr&4),0x93e2))
    N(0x93aa,2,r[2].rh&=0x3f;value(h,r[2].rh))
    N(0x93ac,2,r[2].rl|=0x1;value(h,r[2].rl))
    N(0x93ae,2,f.move(r[2].e,r[2].r))
    N(0x93b0,4,f.move(r[2].r,0xffff))
    N(0x93b4,10,f.store32(r[3].er+8,r[2].er))
    N(0x93be,10,f.move(r[6].er,h.read32(r[3].er+0)))
    N(0x93c8,4,f.call(0x77f4))
    N(0x93cc,2,f.pop(r[0].r))
    N(0x93ce,2,f.push(r[0].r))
    N(0x93d0,2,f.branch(bool(h.ccr&4),0x93de))
    N(0x93d2,4,f.move(r[1].e,0x1000))
    N(0x93d6,4,f.move(r[4].r,0x72))
    N(0x93da,4,f.call(0x785a))
    N(0x93de,4,f.call(0x8fc0))
    N(0x93e2,6,f.move(r[0].rl,h.read8(0x174a7)))
    N(0x93e8,2,f.inc(r[0].rl,1))
    N(0x93ea,6,f.store8(0x174a7,r[0].rl))
    N(0x93f0,2,f.pop(r[2].r))
    N(0x93f2,2,f.pop(r[1].r))
    N(0x93f4,2,f.pop(r[0].r))
    N(0x93f6,2,f.inc(r[0].rl,-1))
    N(0x93f8,2,f.branch(!(h.ccr&4),0x938a))
    N(0x93fa,4,f.call(0x30b8))
    N(0x93fe,2,f.ret())
    N(0x9400,6,f.move(r[1].rl,h.read8(0x17526)))
    N(0x9406,2,f.move(r[0].rl,0x0))
    N(0x9408,6,f.store8(0x174a7,r[0].rl))
    N(0x940e,6,f.move(r[0].rl,h.read8(0x174a6)))
    N(0x9414,2,f.push(r[0].r))
    N(0x9416,2,f.push(r[1].r))
    N(0x9418,4,f.call(0xa656))
    N(0x941c,8,f.move(r[2].r,h.read16(r[3].er+8)))
    N(0x9424,2,f.branch(bool(h.ccr&4),0x946c))
    N(0x9426,8,f.move(r[0].rl,h.read8(r[3].er+4)))
    N(0x942e,2,h.sub8(r[1].rl,r[0].rl))
    N(0x9430,2,f.branch(!(h.ccr&4),0x946c))
    N(0x9432,8,f.move(r[0].rl,h.read8(r[3].er+17)))
    N(0x943a,2,f.branch(bool(h.ccr&4),0x946c))
    N(0x943c,2,f.move(r[0].rl,0x0))
    N(0x943e,8,f.store8(r[3].er+17,r[0].rl))
    N(0x9446,2,r[2].rh&=0x3f;value(h,r[2].rh))
    N(0x9448,2,r[2].rl|=0x1;value(h,r[2].rl))
    N(0x944a,2,f.move(r[2].e,r[2].r))
    N(0x944c,4,f.move(r[2].r,0xffff))
    N(0x9450,10,f.store32(r[3].er+8,r[2].er))
    N(0x945a,10,f.move(r[6].er,h.read32(r[3].er+0)))
    N(0x9464,4,f.call(0x77f4))
    N(0x9468,4,f.call(0x8fc0))
    N(0x946c,6,f.move(r[0].rl,h.read8(0x174a7)))
    N(0x9472,2,f.inc(r[0].rl,1))
    N(0x9474,6,f.store8(0x174a7,r[0].rl))
    N(0x947a,2,f.pop(r[1].r))
    N(0x947c,2,f.pop(r[0].r))
    N(0x947e,2,f.inc(r[0].rl,-1))
    N(0x9480,2,f.branch(!(h.ccr&4),0x9414))
    N(0x9482,2,f.ret())
    N(0x92c0,2,f.move(r[1].rh,r[5].rl))
    N(0x92c2,6,f.move(r[1].rl,h.read8(0x17526)))
    N(0x92c8,2,f.move(r[0].rl,0x0))
    N(0x92ca,6,f.store8(0x174a7,r[0].rl))
    N(0x92d0,6,f.move(r[0].rl,h.read8(0x174a6)))
    N(0x92d6,2,f.push(r[0].r))
    N(0x92d8,2,f.push(r[1].r))
    N(0x92da,4,f.call(0xa656))
    N(0x92de,8,f.move(r[2].r,h.read16(r[3].er+8)))
    N(0x92e6,2,f.branch(bool(h.ccr&4),0x9354))
    N(0x92e8,8,f.move(r[0].rl,h.read8(r[3].er+4)))
    N(0x92f0,2,h.sub8(r[1].rl,r[0].rl))
    N(0x92f2,2,f.branch(!(h.ccr&4),0x9354))
    N(0x92f4,8,f.move(r[0].rl,h.read8(r[3].er+5)))
    N(0x92fc,2,h.sub8(r[1].rh,r[0].rl))
    N(0x92fe,2,f.branch(!(h.ccr&4),0x9354))
    N(0x9300,8,f.move(r[0].rl,h.read8(r[3].er+18)))
    N(0x9308,2,f.branch(!(h.ccr&4),0x9354))
    N(0x930a,10,f.move(r[6].er,h.read32(r[3].er+0)))
    N(0x9314,8,f.move(r[0].rl,h.read8(r[6].er+88)))
    N(0x931c,2,flag(h,4,(uint8_t(r[0].rl)&(1u<<0x2))==0))
    N(0x931e,2,f.branch(!(h.ccr&4),0x9338))
    N(0x9320,4,f.call(0x4000))
    N(0x9324,8,f.move(r[0].rl,h.read8(r[0].er+309)))
    N(0x932c,2,f.branch(bool(h.ccr&4),0x9338))
    N(0x932e,8,f.store8(r[3].er+17,r[0].rl))
    N(0x9336,2,f.branch(true,0x9354))
    N(0x9338,2,r[2].rh&=0x3f;value(h,r[2].rh))
    N(0x933a,2,r[2].rl|=0x1;value(h,r[2].rl))
    N(0x933c,2,f.move(r[2].e,r[2].r))
    N(0x933e,4,f.move(r[2].r,0xffff))
    N(0x9342,10,f.store32(r[3].er+8,r[2].er))
    N(0x934c,4,f.call(0x77f4))
    N(0x9350,4,f.call(0x8fc0))
    N(0x9354,6,f.move(r[0].rl,h.read8(0x174a7)))
    N(0x935a,2,f.inc(r[0].rl,1))
    N(0x935c,6,f.store8(0x174a7,r[0].rl))
    N(0x9362,2,f.pop(r[1].r))
    N(0x9364,2,f.pop(r[0].r))
    N(0x9366,2,f.inc(r[0].rl,-1))
    N(0x9368,4,f.branch(!(h.ccr&4),0x92d6))
    N(0x936c,2,f.ret())
    N(0x936e,4,f.move(r[2].r,0x1))
    N(0x9372,2,f.branch(true,0x9376))
 default:return false;
 }
#undef N
}
bool partialUnison(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x8fcc,6,f.store8(0x17528,r[5].rl))
    N(0x8fd2,6,f.store8(0x17529,r[5].rh))
    N(0x8fd8,4,f.call(0xa66a))
    N(0x8fdc,10,f.move(r[6].er,h.read32(r[0].er+0)))
    N(0x8fe6,8,f.move(r[1].rl,h.read8(r[6].er+0)))
    N(0x8fee,2,r[1].rl&=0x7;value(h,r[1].rl))
    N(0x8ff0,2,f.branch(bool(h.ccr&4),0x902a))
    N(0x8ff2,4,f.call(0xa66a))
    N(0x8ff6,8,f.move(r[1].rh,h.read8(r[0].er+18)))
    N(0x8ffe,2,r[1].rh&=0xfffffff8;value(h,r[1].rh))
    N(0x9000,2,r[1].rh|=r[1].rl;value(h,r[1].rh))
    N(0x9002,8,f.store8(r[0].er+18,r[1].rh))
    N(0x900a,4,f.push(r[0].er))
    N(0x900e,2,f.call(0x902a))
    N(0x9010,4,f.pop(r[0].er))
    N(0x9014,8,f.move(r[1].rl,h.read8(r[0].er+18)))
    N(0x901c,2,f.inc(r[1].rl,-1))
    N(0x901e,8,f.store8(r[0].er+18,r[1].rl))
    N(0x9026,2,r[1].rl&=0x7;value(h,r[1].rl))
    N(0x9028,2,f.branch(!(h.ccr&4),0x900a))
 default:return false;
 }
#undef N
}
bool partialVoiceTracking(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0xa1b0,6,h.sub32(r[6].er,0x839000))
    N(0xa1b6,2,f.branch(!(h.ccr&1),0xa1d8))
    N(0xa1b8,6,f.move(r[0].rl,h.read8(0x17526)))
    N(0xa1be,6,f.move(r[0].rh,h.read8(0x16798)))
    N(0xa1c4,2,h.sub8(r[0].rh,r[0].rl))
    N(0xa1c6,2,f.branch(bool(h.ccr&4),0xa1e2))
    N(0xa1c8,6,f.move(r[0].rl,h.read8(0x174a7)))
    N(0xa1ce,6,f.move(r[0].rh,h.read8(0x1755e)))
    N(0xa1d4,2,h.sub8(r[0].rh,r[0].rl))
    N(0xa1d6,2,f.branch(!(h.ccr&4),0xa1e0))
    N(0xa1d8,2,f.move(r[0].rl,0xffffffff))
    N(0xa1da,6,f.store8(0x1755e,r[0].rl))
    N(0xa1e0,2,f.ret())
    N(0xa1e2,4,f.move(r[4].r,0x8046))
    N(0xa1e6,6,f.move(r[1].rh,h.read8(0x174a7)))
    N(0xa1ec,6,f.store8(0x1755e,r[1].rh))
    N(0xa1f2,2,r[1].e=h.sub16(r[1].e,r[1].e))
    N(0xa1f4,4,f.call(0x24be))
    N(0xa1f8,2,f.ret())
    N(0xa1fa,4,f.move(r[4].r,0x8045))
    N(0xa1fe,2,f.move(r[1].rh,r[6].rh))
    N(0xa200,2,r[1].e=h.sub16(r[1].e,r[1].e))
    N(0xa202,4,f.call(0x24be))
    N(0xa206,2,f.ret())
    N(0xa208,6,f.move(r[0].rl,h.read8(0x17526)))
    N(0xa20e,6,f.move(r[0].rh,h.read8(0x16798)))
    N(0xa214,2,h.sub8(r[0].rh,r[0].rl))
    N(0xa216,4,f.branch(!(h.ccr&4),0xa336))
    N(0xa21a,6,f.move(r[0].rl,h.read8(0x1755e)))
    N(0xa220,4,f.branch(bool(h.ccr&8),0xa336))
    N(0xa224,6,f.store8(0x174a7,r[0].rl))
    N(0xa22a,4,f.call(0x77f4))
    N(0xa22e,8,f.move(r[6].er,h.read32(0x167bc)))
    N(0xa236,8,f.move(r[2].rl,h.read8(r[6].er+73)))
    N(0xa23e,2,f.branch(bool(h.ccr&4),0xa260))
    N(0xa240,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0xa242,4,f.move(r[4].r,0x7e))
    N(0xa246,4,f.call(0x785a))
    N(0xa24a,2,f.inc(r[2].rl,-1))
    N(0xa24c,2,f.shift(r[2].rl,1,true))
    N(0xa24e,2,f.shift(r[2].rl,1,true))
    N(0xa250,2,r[2].r&=0xff;value(h,r[2].r))
    N(0xa252,2,r[2].er&=0xffff;value(h,r[2].er))
    N(0xa254,6,r[2].er=h.add32(r[2].er,0x159f4))
    N(0xa25a,4,f.move(r[1].er,h.read32(r[2].er)))
    N(0xa25e,2,f.branch(true,0xa2ae))
    N(0xa260,8,f.move(r[0].rh,h.read8(r[6].er+72)))
    N(0xa268,2,f.move(r[0].rl,0x0))
    N(0xa26a,2,f.push(r[0].r))
    N(0xa26c,2,f.shift(r[0].r,1,false))
    N(0xa26e,4,f.move(r[0].e,0xaaab))
    N(0xa272,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20))
    N(0xa274,2,f.move(r[0].r,r[0].e))
    N(0xa276,4,r[0].r=h.add16(r[0].r,0x4700))
    N(0xa27a,6,f.move(r[0].e,h.read16(0x15138)))
    N(0xa280,4,r[0].e=h.sub16(r[0].e,0x4470))
    N(0xa284,2,r[0].r=h.add16(r[0].r,r[0].e))
    N(0xa286,4,f.call(0xa69e))
    N(0xa28a,2,f.pop(r[2].r))
    N(0xa28c,2,h.sub8(r[2].rh,0x20))
    N(0xa28e,2,f.branch(bool(h.ccr&5),0xa292))
    N(0xa290,2,f.move(r[2].rh,0x20))
    N(0xa292,2,f.shift(r[2].rh,1,true))
    N(0xa294,2,f.shift(r[2].rh,1,true))
    N(0xa296,2,f.move(r[1].r,r[0].e))
    N(0xa298,2,r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[2].r));h.clockI(20))
    N(0xa29a,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[2].r));h.clockI(20))
    N(0xa29c,2,f.move(r[0].r,r[0].e))
    N(0xa29e,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0xa2a0,2,r[1].er=h.add32(r[1].er,r[0].er))
    N(0xa2a2,2,f.shift(r[1].er,1,true))
    N(0xa2a4,4,f.move(r[4].r,0x7e))
    N(0xa2a8,4,f.call(0x785a))
    N(0xa2ac,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0xa2ae,4,f.move(r[4].r,0x7f))
    N(0xa2b2,4,f.call(0x785a))
    N(0xa2b6,8,f.move(r[2].rl,h.read8(r[6].er+76)))
    N(0xa2be,2,f.branch(bool(h.ccr&4),0xa2e0))
    N(0xa2c0,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0xa2c2,4,f.move(r[4].r,0x86))
    N(0xa2c6,4,f.call(0x785a))
    N(0xa2ca,2,f.inc(r[2].rl,-1))
    N(0xa2cc,2,f.shift(r[2].rl,1,true))
    N(0xa2ce,2,f.shift(r[2].rl,1,true))
    N(0xa2d0,2,r[2].r&=0xff;value(h,r[2].r))
    N(0xa2d2,2,r[2].er&=0xffff;value(h,r[2].er))
    N(0xa2d4,6,r[2].er=h.add32(r[2].er,0x159f4))
    N(0xa2da,4,f.move(r[1].er,h.read32(r[2].er)))
    N(0xa2de,2,f.branch(true,0xa32e))
    N(0xa2e0,8,f.move(r[0].rh,h.read8(r[6].er+75)))
    N(0xa2e8,2,f.move(r[0].rl,0x0))
    N(0xa2ea,2,f.push(r[0].r))
    N(0xa2ec,2,f.shift(r[0].r,1,false))
    N(0xa2ee,4,f.move(r[0].e,0xaaab))
    N(0xa2f2,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20))
    N(0xa2f4,2,f.move(r[0].r,r[0].e))
    N(0xa2f6,4,r[0].r=h.add16(r[0].r,0x4700))
    N(0xa2fa,6,f.move(r[0].e,h.read16(0x15138)))
    N(0xa300,4,r[0].e=h.sub16(r[0].e,0x4470))
    N(0xa304,2,r[0].r=h.add16(r[0].r,r[0].e))
    N(0xa306,4,f.call(0xa69e))
    N(0xa30a,2,f.pop(r[2].r))
    N(0xa30c,2,h.sub8(r[2].rh,0x20))
    N(0xa30e,2,f.branch(bool(h.ccr&5),0xa312))
    N(0xa310,2,f.move(r[2].rh,0x20))
    N(0xa312,2,f.shift(r[2].rh,1,true))
    N(0xa314,2,f.shift(r[2].rh,1,true))
    N(0xa316,2,f.move(r[1].r,r[0].e))
    N(0xa318,2,r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[2].r));h.clockI(20))
    N(0xa31a,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[2].r));h.clockI(20))
    N(0xa31c,2,f.move(r[0].r,r[0].e))
    N(0xa31e,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0xa320,2,r[1].er=h.add32(r[1].er,r[0].er))
    N(0xa322,2,f.shift(r[1].er,1,true))
    N(0xa324,4,f.move(r[4].r,0x86))
    N(0xa328,4,f.call(0x785a))
    N(0xa32c,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0xa32e,4,f.move(r[4].r,0x87))
    N(0xa332,4,f.call(0x785a))
    N(0xa336,2,f.ret())
 default:return false;
 }
#undef N
}
bool partialVelocityModulation(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{RuntimeBoundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x8932,4,f.call(0xa656))
    N(0x8936,8,f.move(r[1].rl,h.read8(r[3].er+6)))
    N(0x893e,8,f.move(r[1].rh,h.read8(r[6].er+58)))
    N(0x8946,2,r[1].rh=h.add8(r[1].rh,0xffffffc0))
    N(0x8948,2,f.branch(!(h.ccr&8),0x894e))
    N(0x894a,2,r[1].rl^=0x7f;value(h,r[1].rl))
    N(0x894c,2,r[1].rh=h.sub8(0,r[1].rh))
    N(0x894e,2,r[1].rl=h.add8(r[1].rl,0xffffff81))
    N(0x8950,4,f.multiplySigned(r[1].rh,r[1].r))
    N(0x8954,2,f.shift(r[1].r,2,true))
    N(0x8956,2,f.branch(!(h.ccr&4),0x8960))
    N(0x8958,6,f.move(r[1].er,0x7fffff00))
    N(0x895e,2,f.branch(true,0x8966))
    N(0x8960,2,r[1].rh=h.add8(r[1].rh,0x7f))
    N(0x8962,2,f.move(r[1].e,r[1].r))
    N(0x8964,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x8966,4,f.call(0x2520))
    N(0x89f4,4,f.call(0xa656))
    N(0x89f8,8,f.move(r[1].rl,h.read8(r[3].er+6)))
    N(0x8a00,8,f.move(r[1].rh,h.read8(r[6].er+61)))
    N(0x8a08,2,r[1].rh=h.add8(r[1].rh,0xffffffc0))
    N(0x8a0a,2,f.branch(!(h.ccr&8),0x8a10))
    N(0x8a0c,2,r[1].rl^=0x7f;value(h,r[1].rl))
    N(0x8a0e,2,r[1].rh=h.sub8(0,r[1].rh))
    N(0x8a10,2,r[1].rl=h.add8(r[1].rl,0xffffff81))
    N(0x8a12,4,f.multiplySigned(r[1].rh,r[1].r))
    N(0x8a16,2,f.shift(r[1].r,2,true))
    N(0x8a18,2,f.branch(!(h.ccr&4),0x8a22))
    N(0x8a1a,6,f.move(r[1].er,0x7fffff00))
    N(0x8a20,2,f.branch(true,0x8a28))
    N(0x8a22,2,r[1].rh=h.add8(r[1].rh,0x7f))
    N(0x8a24,2,f.move(r[1].e,r[1].r))
    N(0x8a26,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x8a28,4,f.call(0x2520))
    N(0x8b0e,4,f.call(0xa656))
    N(0x8b12,8,f.move(r[1].rl,h.read8(r[3].er+6)))
    N(0x8b1a,8,f.move(r[1].rh,h.read8(r[6].er+66)))
    N(0x8b22,2,r[1].rh=h.add8(r[1].rh,0xffffffc0))
    N(0x8b24,2,f.branch(!(h.ccr&8),0x8b2a))
    N(0x8b26,2,r[1].rl^=0x7f;value(h,r[1].rl))
    N(0x8b28,2,r[1].rh=h.sub8(0,r[1].rh))
    N(0x8b2a,2,r[1].rl=h.add8(r[1].rl,0xffffff81))
    N(0x8b2c,4,f.multiplySigned(r[1].rh,r[1].r))
    N(0x8b30,2,f.shift(r[1].r,2,true))
    N(0x8b32,2,f.branch(!(h.ccr&4),0x8b3c))
    N(0x8b34,6,f.move(r[1].er,0x7fffff00))
    N(0x8b3a,2,f.branch(true,0x8b42))
    N(0x8b3c,2,r[1].rh=h.add8(r[1].rh,0x7f))
    N(0x8b3e,2,f.move(r[1].e,r[1].r))
    N(0x8b40,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x8b42,4,f.call(0x2520))
    N(0x8d06,4,f.call(0xa656))
    N(0x8d0a,8,f.move(r[1].rl,h.read8(r[3].er+6)))
    N(0x8d12,8,f.move(r[1].rh,h.read8(r[6].er+169)))
    N(0x8d1a,2,r[1].rh=h.add8(r[1].rh,0xffffffc0))
    N(0x8d1c,2,f.branch(!(h.ccr&8),0x8d22))
    N(0x8d1e,2,r[1].rl^=0x7f;value(h,r[1].rl))
    N(0x8d20,2,r[1].rh=h.sub8(0,r[1].rh))
    N(0x8d22,2,r[1].rl=h.add8(r[1].rl,0xffffff81))
    N(0x8d24,4,f.multiplySigned(r[1].rh,r[1].r))
    N(0x8d28,2,f.shift(r[1].r,2,true))
    N(0x8d2a,2,f.branch(!(h.ccr&4),0x8d34))
    N(0x8d2c,6,f.move(r[1].er,0x7fffff00))
    N(0x8d32,2,f.branch(true,0x8d3a))
    N(0x8d34,2,r[1].rh=h.add8(r[1].rh,0x7f))
    N(0x8d36,2,f.move(r[1].e,r[1].r))
    N(0x8d38,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x8d3a,4,f.call(0x2520))
    N(0x8e64,4,f.call(0xa656))
    N(0x8e68,8,f.move(r[1].rl,h.read8(r[3].er+6)))
    N(0x8e70,8,f.move(r[1].rh,h.read8(r[6].er+170)))
    N(0x8e78,2,r[1].rh=h.add8(r[1].rh,0xffffffc0))
    N(0x8e7a,2,f.branch(!(h.ccr&8),0x8e80))
    N(0x8e7c,2,r[1].rl^=0x7f;value(h,r[1].rl))
    N(0x8e7e,2,r[1].rh=h.sub8(0,r[1].rh))
    N(0x8e80,2,r[1].rl=h.add8(r[1].rl,0xffffff81))
    N(0x8e82,4,f.multiplySigned(r[1].rh,r[1].r))
    N(0x8e86,2,f.shift(r[1].r,2,true))
    N(0x8e88,2,f.branch(!(h.ccr&4),0x8e92))
    N(0x8e8a,6,f.move(r[1].er,0x7fffff00))
    N(0x8e90,2,f.branch(true,0x8e98))
    N(0x8e92,2,r[1].rh=h.add8(r[1].rh,0x7f))
    N(0x8e94,2,f.move(r[1].e,r[1].r))
    N(0x8e96,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x8e98,4,f.call(0x2520))
    N(0x9084,4,f.call(0xa656))
    N(0x9088,8,f.move(r[1].rl,h.read8(r[3].er+6)))
    N(0x9090,8,f.move(r[1].rh,h.read8(r[6].er+58)))
    N(0x9098,2,r[1].rh=h.add8(r[1].rh,0xffffffc0))
    N(0x909a,2,f.branch(!(h.ccr&8),0x90a0))
    N(0x909c,2,r[1].rl^=0x7f;value(h,r[1].rl))
    N(0x909e,2,r[1].rh=h.sub8(0,r[1].rh))
    N(0x90a0,2,r[1].rl=h.add8(r[1].rl,0xffffff81))
    N(0x90a2,4,r[1].r=int8_t(r[1].rh)*int8_t(r[1].r);flag(h,8,r[1].r<0);flag(h,4,!r[1].r);h.clockI(12))
    N(0x90a6,2,f.shift(r[1].r,2,true))
    N(0x90a8,2,f.branch(!(h.ccr&4),0x90b2))
    N(0x90aa,6,f.move(r[1].er,0x7fffff00))
    N(0x90b0,2,f.branch(true,0x90b8))
    N(0x90b2,2,r[1].rh=h.add8(r[1].rh,0x7f))
    N(0x90b4,2,f.move(r[1].e,r[1].r))
    N(0x90b6,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x90b8,4,f.move(r[4].r,0x6b))
    N(0x90bc,4,f.call(0x785a))
    N(0x90c0,4,f.call(0xa656))
    N(0x90c4,8,f.move(r[1].rl,h.read8(r[3].er+6)))
    N(0x90cc,8,f.move(r[1].rh,h.read8(r[6].er+61)))
    N(0x90d4,2,r[1].rh=h.add8(r[1].rh,0xffffffc0))
    N(0x90d6,2,f.branch(!(h.ccr&8),0x90dc))
    N(0x90d8,2,r[1].rl^=0x7f;value(h,r[1].rl))
    N(0x90da,2,r[1].rh=h.sub8(0,r[1].rh))
    N(0x90dc,2,r[1].rl=h.add8(r[1].rl,0xffffff81))
    N(0x90de,4,r[1].r=int8_t(r[1].rh)*int8_t(r[1].r);flag(h,8,r[1].r<0);flag(h,4,!r[1].r);h.clockI(12))
    N(0x90e2,2,f.shift(r[1].r,2,true))
    N(0x90e4,2,f.branch(!(h.ccr&4),0x90ee))
    N(0x90e6,6,f.move(r[1].er,0x7fffff00))
    N(0x90ec,2,f.branch(true,0x90f4))
    N(0x90ee,2,r[1].rh=h.add8(r[1].rh,0x7f))
    N(0x90f0,2,f.move(r[1].e,r[1].r))
    N(0x90f2,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x90f4,4,f.move(r[4].r,0x6f))
    N(0x90f8,4,f.call(0x785a))
    N(0x90fc,4,f.call(0xa656))
    N(0x9100,8,f.move(r[1].rl,h.read8(r[3].er+6)))
    N(0x9108,8,f.move(r[1].rh,h.read8(r[6].er+66)))
    N(0x9110,2,r[1].rh=h.add8(r[1].rh,0xffffffc0))
    N(0x9112,2,f.branch(!(h.ccr&8),0x9118))
    N(0x9114,2,r[1].rl^=0x7f;value(h,r[1].rl))
    N(0x9116,2,r[1].rh=h.sub8(0,r[1].rh))
    N(0x9118,2,r[1].rl=h.add8(r[1].rl,0xffffff81))
    N(0x911a,4,r[1].r=int8_t(r[1].rh)*int8_t(r[1].r);flag(h,8,r[1].r<0);flag(h,4,!r[1].r);h.clockI(12))
    N(0x911e,2,f.shift(r[1].r,2,true))
    N(0x9120,2,f.branch(!(h.ccr&4),0x912a))
    N(0x9122,6,f.move(r[1].er,0x7fffff00))
    N(0x9128,2,f.branch(true,0x9130))
    N(0x912a,2,r[1].rh=h.add8(r[1].rh,0x7f))
    N(0x912c,2,f.move(r[1].e,r[1].r))
    N(0x912e,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x9130,4,f.move(r[4].r,0x77))
    N(0x9134,4,f.call(0x785a))
 default:return false;
 }
#undef N
}
bool partialGlide(ControllerCpu& h){
 using namespace controllerDetail;auto& r=h.regs;
#define N(P,L,...) case P:{RuntimeBoundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);return true;}
 switch(unsigned(h.getPC())){
    N(0x8e9c,8,f.move(r[0].rh,h.read8(r[6].er+168)))
    N(0x8ea4,2,f.branch(!(h.ccr&4),0x8eb2))
    N(0x8ea6,4,f.call(0xa656))
    N(0x8eaa,8,f.move(r[0].rh,h.read8(r[3].er+13)))
    N(0x8eb2,2,r[0].rh=h.add8(r[0].rh,0xffffffc4))
    N(0x8eb4,2,r[0].rl=h.sub8(r[0].rl,r[0].rl))
    N(0x8eb6,2,f.move(r[1].e,r[0].r))
    N(0x8eb8,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x8eba,4,f.call(0x2520))
    N(0x8ebe,8,f.move(r[0].rh,h.read8(r[6].er+4)))
    N(0x8ec6,2,r[0].rh=h.add8(r[0].rh,0xffffffc0))
    N(0x8ec8,2,f.branch(bool(h.ccr&4),0x8eec))
    N(0x8eca,2,f.move(r[0].rl,r[0].rh))
    N(0x8ecc,2,f.branch(!(h.ccr&8),0x8ed0))
    N(0x8ece,2,r[0].rl=h.sub8(0,r[0].rl))
    N(0x8ed0,4,f.call(0xa656))
    N(0x8ed4,8,f.store8(r[3].er+7,r[0].rl))
    N(0x8edc,8,f.move(r[0].rl,h.read8(r[3].er+13)))
    N(0x8ee4,2,r[0].rl=h.add8(r[0].rl,r[0].rh))
    N(0x8ee6,6,f.store8(0x1752d,r[0].rl))
    N(0x8eec,8,f.move(r[0].rl,h.read8(r[6].er+3)))
    N(0x8ef4,8,f.move(r[2].rh,h.read8(r[6].er+0)))
    N(0x8efc,2,f.shift(r[2].rh,2,false))
    N(0x8efe,2,f.shift(r[2].rh,1,false))
    N(0x8f00,2,r[2].rh&=0x3;value(h,r[2].rh))
    N(0x8f02,2,h.sub8(r[2].rh,0x1))
    N(0x8f04,2,f.branch(!(h.ccr&4),0x8f16))
    N(0x8f06,6,f.move(r[2].rl,h.read8(0x1752a)))
    N(0x8f0c,2,h.sub8(r[2].rl,0x1))
    N(0x8f0e,2,f.branch(bool(h.ccr&4),0x8f14))
    N(0x8f10,2,h.sub8(r[2].rl,0x2))
    N(0x8f12,2,f.branch(!(h.ccr&4),0x8f16))
    N(0x8f14,2,r[0].rl=h.sub8(r[0].rl,r[0].rl))
    N(0x8f16,2,f.move(r[0].rl,r[0].rl))
    N(0x8f18,2,f.branch(!(h.ccr&4),0x8f2e))
    N(0x8f1a,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0x8f1c,4,f.call(0x2520))
    N(0x8f20,6,f.move(r[1].er,0x7fffff00))
    N(0x8f26,4,f.call(0x2520))
    N(0x8f2a,4,f.branch(true,0x8fa4))
    N(0x8f2e,2,r[0].rl^=0x7f;value(h,r[0].rl))
    N(0x8f30,2,f.move(r[2].rh,r[0].rl))
    N(0x8f32,2,f.move(r[0].rh,0x5d))
    N(0x8f34,2,r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12))
    N(0x8f36,2,f.move(r[2].rl,0x76))
    N(0x8f38,2,r[2].rh=h.sub8(r[2].rh,r[2].rl))
    N(0x8f3a,2,f.branch(bool(h.ccr&5),0x8f40))
    N(0x8f3c,2,r[2].rl=h.sub8(r[2].rl,r[2].rl))
    N(0x8f3e,2,r[0].r=h.add16(r[0].r,r[2].r))
    N(0x8f40,6,f.move(r[0].e,h.read16(0x1513c)))
    N(0x8f46,2,r[0].r=h.add16(r[0].r,r[0].e))
    N(0x8f48,4,f.call(0xa69e))
    N(0x8f4c,8,f.move(r[1].rl,h.read8(r[6].er+5)))
    N(0x8f54,2,f.branch(bool(h.ccr&4),0x8f8a))
    N(0x8f56,4,f.call(0xa656))
    N(0x8f5a,8,f.move(r[2].rh,h.read8(r[3].er+7)))
    N(0x8f62,2,f.branch(bool(h.ccr&4),0x8f8a))
    N(0x8f64,2,r[2].rl=h.sub8(r[2].rl,r[2].rl))
    N(0x8f66,2,f.move(r[1].r,r[2].r))
    N(0x8f68,2,r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[0].e));h.clockI(20))
    N(0x8f6a,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[2].r));h.clockI(20))
    N(0x8f6c,2,f.move(r[0].r,r[0].e))
    N(0x8f6e,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0x8f70,2,r[1].er=h.add32(r[1].er,r[0].er))
    N(0x8f72,6,h.sub32(r[1].er,0x200))
    N(0x8f78,2,f.branch(!(h.ccr&1),0x8f7e))
    N(0x8f7a,4,f.move(r[1].r,0x200))
    N(0x8f7e,4,f.call(0x2520))
    N(0x8f82,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0x8f84,4,f.call(0x2520))
    N(0x8f88,2,f.branch(true,0x8fa4))
    N(0x8f8a,2,f.move(r[1].er,r[0].er))
    N(0x8f8c,4,f.push(r[1].er))
    N(0x8f90,2,f.move(r[0].rl,0x5))
    N(0x8f92,2,f.shift(r[1].er,1,false))
    N(0x8f94,2,f.inc(r[0].rl,-1))
    N(0x8f96,2,f.branch(!(h.ccr&4),0x8f92))
    N(0x8f98,4,f.call(0x2520))
    N(0x8f9c,4,f.pop(r[1].er))
    N(0x8fa0,4,f.call(0x2520))
    N(0x8fa4,6,f.move(r[0].rh,h.read8(0x1752d)))
    N(0x8faa,2,r[0].rh=h.add8(r[0].rh,0xffffffc4))
    N(0x8fac,2,r[0].rl=h.sub8(r[0].rl,r[0].rl))
    N(0x8fae,2,f.move(r[1].e,r[0].r))
    N(0x8fb0,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x8fb2,4,f.call(0x2520))
    N(0x9138,8,f.move(r[0].rh,h.read8(r[6].er+168)))
    N(0x9140,2,f.branch(!(h.ccr&4),0x914e))
    N(0x9142,4,f.call(0xa656))
    N(0x9146,8,f.move(r[0].rh,h.read8(r[3].er+13)))
    N(0x914e,2,r[0].rh=h.add8(r[0].rh,0xffffffc4))
    N(0x9150,2,r[0].rl=h.sub8(r[0].rl,r[0].rl))
    N(0x9152,2,f.move(r[1].e,r[0].r))
    N(0x9154,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x9156,4,f.move(r[4].r,0x8c))
    N(0x915a,4,f.call(0x785a))
    N(0x915e,8,f.move(r[0].rh,h.read8(r[6].er+4)))
    N(0x9166,2,r[0].rh=h.add8(r[0].rh,0xffffffc0))
    N(0x9168,2,f.branch(bool(h.ccr&4),0x918c))
    N(0x916a,2,f.move(r[0].rl,r[0].rh))
    N(0x916c,2,f.branch(!(h.ccr&8),0x9170))
    N(0x916e,2,r[0].rl=h.sub8(0,r[0].rl))
    N(0x9170,4,f.call(0xa656))
    N(0x9174,8,f.store8(r[3].er+7,r[0].rl))
    N(0x917c,8,f.move(r[0].rl,h.read8(r[3].er+13)))
    N(0x9184,2,r[0].rl=h.add8(r[0].rl,r[0].rh))
    N(0x9186,6,f.store8(0x1752d,r[0].rl))
    N(0x918c,8,f.move(r[0].rl,h.read8(r[6].er+3)))
    N(0x9194,8,f.move(r[2].rh,h.read8(r[6].er+0)))
    N(0x919c,2,f.shift(r[2].rh,2,false))
    N(0x919e,2,f.shift(r[2].rh,1,false))
    N(0x91a0,2,r[2].rh&=0x3;value(h,r[2].rh))
    N(0x91a2,2,h.sub8(r[2].rh,0x1))
    N(0x91a4,2,f.branch(!(h.ccr&4),0x91b6))
    N(0x91a6,6,f.move(r[2].rl,h.read8(0x1752a)))
    N(0x91ac,2,h.sub8(r[2].rl,0x1))
    N(0x91ae,2,f.branch(bool(h.ccr&4),0x91b4))
    N(0x91b0,2,h.sub8(r[2].rl,0x2))
    N(0x91b2,2,f.branch(!(h.ccr&4),0x91b6))
    N(0x91b4,2,r[0].rl=h.sub8(r[0].rl,r[0].rl))
    N(0x91b6,2,f.move(r[0].rl,r[0].rl))
    N(0x91b8,2,f.branch(!(h.ccr&4),0x91d6))
    N(0x91ba,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0x91bc,4,f.move(r[4].r,0x8d))
    N(0x91c0,4,f.call(0x785a))
    N(0x91c4,6,f.move(r[1].er,0x7fffff00))
    N(0x91ca,4,f.move(r[4].r,0x8e))
    N(0x91ce,4,f.call(0x785a))
    N(0x91d2,4,f.branch(true,0x925c))
    N(0x91d6,2,r[0].rl^=0x7f;value(h,r[0].rl))
    N(0x91d8,2,f.move(r[2].rh,r[0].rl))
    N(0x91da,2,f.move(r[0].rh,0x5d))
    N(0x91dc,2,r[0].r=uint8_t(r[0].r)*uint32_t(uint8_t(r[0].rh));h.clockI(12))
    N(0x91de,2,f.move(r[2].rl,0x76))
    N(0x91e0,2,r[2].rh=h.sub8(r[2].rh,r[2].rl))
    N(0x91e2,2,f.branch(bool(h.ccr&5),0x91e8))
    N(0x91e4,2,r[2].rl=h.sub8(r[2].rl,r[2].rl))
    N(0x91e6,2,r[0].r=h.add16(r[0].r,r[2].r))
    N(0x91e8,6,f.move(r[0].e,h.read16(0x1513c)))
    N(0x91ee,2,r[0].r=h.add16(r[0].r,r[0].e))
    N(0x91f0,4,f.call(0xa69e))
    N(0x91f4,8,f.move(r[1].rl,h.read8(r[6].er+5)))
    N(0x91fc,2,f.branch(bool(h.ccr&4),0x923a))
    N(0x91fe,4,f.call(0xa656))
    N(0x9202,8,f.move(r[2].rh,h.read8(r[3].er+7)))
    N(0x920a,2,f.branch(bool(h.ccr&4),0x923a))
    N(0x920c,2,r[2].rl=h.sub8(r[2].rl,r[2].rl))
    N(0x920e,2,f.move(r[1].r,r[2].r))
    N(0x9210,2,r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[0].e));h.clockI(20))
    N(0x9212,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[2].r));h.clockI(20))
    N(0x9214,2,f.move(r[0].r,r[0].e))
    N(0x9216,2,r[0].e=h.sub16(r[0].e,r[0].e))
    N(0x9218,2,r[1].er=h.add32(r[1].er,r[0].er))
    N(0x921a,6,h.sub32(r[1].er,0x200))
    N(0x9220,2,f.branch(!(h.ccr&1),0x9226))
    N(0x9222,4,f.move(r[1].r,0x200))
    N(0x9226,4,f.move(r[4].r,0x8d))
    N(0x922a,4,f.call(0x785a))
    N(0x922e,2,r[1].er=h.sub32(r[1].er,r[1].er))
    N(0x9230,4,f.move(r[4].r,0x8e))
    N(0x9234,4,f.call(0x785a))
    N(0x9238,2,f.branch(true,0x925c))
    N(0x923a,2,f.move(r[1].er,r[0].er))
    N(0x923c,4,f.push(r[1].er))
    N(0x9240,2,f.move(r[0].rl,0x5))
    N(0x9242,2,f.shift(r[1].er,1,false))
    N(0x9244,2,f.inc(r[0].rl,-1))
    N(0x9246,2,f.branch(!(h.ccr&4),0x9242))
    N(0x9248,4,f.move(r[4].r,0x8d))
    N(0x924c,4,f.call(0x785a))
    N(0x9250,4,f.pop(r[1].er))
    N(0x9254,4,f.move(r[4].r,0x8e))
    N(0x9258,4,f.call(0x785a))
    N(0x925c,6,f.move(r[0].rh,h.read8(0x1752d)))
    N(0x9262,2,r[0].rh=h.add8(r[0].rh,0xffffffc4))
    N(0x9264,2,r[0].rl=h.sub8(r[0].rl,r[0].rl))
    N(0x9266,2,f.move(r[1].e,r[0].r))
    N(0x9268,2,r[1].rh=h.sub8(r[1].rh,r[1].rh))
    N(0x926a,4,f.move(r[4].r,0x8f))
    N(0x926e,4,f.call(0x785a))
    N(0x9272,6,f.move(r[0].rl,h.read8(0x1752a)))
    N(0x9278,2,h.sub8(r[0].rl,0x2))
    N(0x927a,4,f.branch(bool(h.ccr&5),0x9054))
 default:return false;
 }
#undef N
}
struct Install {Install(){setControllerReferenceFactoryForTests(&create);setKeySearchPartialForTests(&partialKeys);setParameterScalePartialForTests(&partialScale);setControlJobsPartialForTests(&historicalControl::service);setDspWordPartialForTests(&partialWord);setDspTransportPartialBatchForTests(&historicalTransport::service);setDspAddressPartialForTests(&partialAddress);setDspFaultPartialForTests(&partialFault);setDspBootStreamPartialForTests(&partialBootStream);setDspBootSetupPartialForTests(&partialBootSetup);setDspStartupPartialForTests(&partialStartup);setDspRoutingPartialForTests(&partialRouting);setPartControlPartialForTests(&partialPartControl);setVelocityCurvePartialForTests(&partialVelocityCurve);setVoiceSelectionPartialForTests(&partialVoiceSelection);setVoicePopulationPartialForTests(&partialVoicePopulation);setKeyResetPartialForTests(&partialKeyReset);setVoiceReleasePartialForTests(&partialVoiceRelease);setUnisonPartialForTests(&partialUnison);setVoiceTrackingPartialForTests(&partialVoiceTracking);setVelocityModulationPartialForTests(&partialVelocityModulation);setGlidePartialForTests(&partialGlide);}} install;
}
}
