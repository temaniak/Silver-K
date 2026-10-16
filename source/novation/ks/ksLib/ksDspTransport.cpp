#include "ksDspTransport.h"
#include "ksControllerBoundary.h"

namespace ks {
namespace {
// Verified low-SRAM code bus: aligned 16-bit fetches cost three clocks each.
// This accounts for protocol timing without fetching/decoding guest code.
// Every data/HDI access still goes through ControllerCpu and the actual Board.
void timing(ControllerCpu& h,unsigned pc,unsigned bytes) {
    h.cycles += 3 * (bytes / 2);
    h.lastread = int(pc + bytes - 1);
    h.pc = h.makepc(pc + 1); // Original in-flight PC seen by the port recorder.
}
void portWrite(ControllerCpu& h,unsigned pc,unsigned offset,int8_t v) {
    timing(h,pc,8);h.write8(v,h.regs[5].er+offset);
    controllerDetail::value(h,v);
}
}
bool DspTransport::candidate(unsigned pc) {
    switch(pc) {
    case 0x2416:case 0x2424:case 0x245e:case 0x246c:
    case 0x24d8:case 0x24e6:case 0x2520:case 0x252e:
    case 0x256e:case 0x257c:case 0x25b6:case 0x25c4:
    case 0x243a:case 0x2482:case 0x2592:case 0x25da:
    case 0x24fc:case 0x2544:case 0x25e4:return true;
    default:return false;
    }
}
namespace {
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
}
unsigned DspTransport::service(ControllerCpu& h,uint64_t deadline) {
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
