#include "ksControllerJobs.h"
#include "ksControllerBoundary.h"
#include "ksControllerScheduling.h"
#include <algorithm>

namespace ks {
namespace {
ControlShiftPartialTest partialShiftTest=nullptr;
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
void setControlShiftPartialForTests(ControlShiftPartialTest fn){partialShiftTest=fn;}
ControllerJobs::Result ControllerJobs::service(ControllerCpu& h,uint64_t deadline){
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
    if(entry==0xa6be||entry==0xa6c6)return partialShiftTest?partialShiftTest(h,deadline):Result{};
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
