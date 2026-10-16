// Original H8 emulator adapter. This translation unit is linked exclusively
// into comparison/test executables, never ksLib or a plugin target.
#include "ksControllerState.h"
#include "h8s/h8s.hpp"
#include "ksKeySearchCalls.h"
#include "ksParameterScaleCalls.h"
#include "ksControllerJobs.h"
#include "ksDspWordCalls.h"
#include "ksDspAddressCalls.h"
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
struct Install {Install(){setControllerReferenceFactoryForTests(&create);setKeySearchPartialForTests(&partialKeys);setParameterScalePartialForTests(&partialScale);setControlShiftPartialForTests(&partialShift);setDspWordPartialForTests(&partialWord);setDspTransportPartialBatchForTests(&historicalTransport::service);setDspAddressPartialForTests(&partialAddress);}} install;
}
}
