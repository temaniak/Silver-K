#include "ksLiveVoiceController.h"
#include "ksControllerBoundary.h"
#include <stdexcept>

namespace ks {
using namespace controllerDetail;
void LiveVoiceController::verify(const ControllerCpu& h){
    uint64_t hash=14695981039346656037ull;
    for(auto range:{std::pair<unsigned,unsigned>{0x2416,0x25fe},{0x77dc,0x7872},
                   {0x8fc0,0x9374},{0xa1b0,0xa338},{0xa338,0xa656},{0xa67e,0xa69e}})
        for(unsigned n=range.first;n<range.second;++n){hash^=h.memory[n];hash*=1099511628211ull;}
    if(hash!=0x418737a410bf4738ull)throw std::runtime_error("Native KS voice lifecycle requires verified OS 2.1.02");
}
bool LiveVoiceController::step(ControllerCpu& h){
    auto& r=h.regs;const unsigned pc=h.getPC();
    if(pc>=0xa67e && PartControlCalls::candidate(pc)){if(!partControlCalls.step(h))return false;++steps;return true;}
    if(VoiceSelectionCalls::candidate(pc)){if(!selectionCalls.step(h))return false;++steps;if(pc==0xa338)++allocationCalls;if(pc==0x8fc0)++releaseCalls;return true;}
    if(VelocityModulationCalls::candidate(pc)&&modulationCalls.step(h)){++steps;return true;}
    if(GlideCalls::candidate(pc)&&glideCalls.step(h)){++steps;return true;}
    if(VoiceTrackingCalls::candidate(pc)){if(!trackingCalls.step(h))return false;++steps;return true;}
    if(UnisonCalls::candidate(pc)){if(!unisonCalls.step(h))return false;++steps;if(pc==0x8fcc)++startCalls;return true;}
    if(VoicePopulationCalls::candidate(pc)){if(!populationCalls.step(h))return false;++steps;return true;}
    if(VelocityCurveCalls::candidate(pc)){if(!velocityCurveCalls.step(h))return false;++steps;return true;}
    if(DspRoutingCalls::candidate(pc)){
        if(!routingCalls.step(h))return false;
        ++steps;if(pc<0x25fe)++transportSteps;return true;
    }
    if(DspSelectCalls::candidate(pc)){
        if(!selectCalls.step(h))return false;
        ++steps;++transportSteps;return true;
    }
    if(DspAddressCalls::candidate(pc)){
        if(!addressCalls.step(h))return false;
        ++steps;++transportSteps;return true;
    }
    if(DspWordCalls::candidate(pc)){
        if(!wordCalls.step(h))return false;
        ++steps;++transportSteps;return true;
    }
    if(pc==0xa338)++allocationCalls;
    if(pc==0x8fcc)++startCalls;
    if(pc==0x8fc0)++releaseCalls;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);++steps;if(P<0x25fe)++transportSteps;return true;}
    switch(pc){
    // Select DSP B/A and invalidate the sequential address cache.
    // Host write: select address, wait for transmit space, then write the
    // high/middle/low bytes of ER1 >> 8 and advance the address mirror.
    // 0x77dc..0x7872: verified interruptible controller boundaries.
    // Voice/part address windows. Negative R4 selects the alternate
    // control region; selection and offset mirrors remain guest-visible.
    // 0x8fc0..0x9374: verified interruptible controller boundaries.
    // Release the selected DSP voice by writing zero to its gate command.
    // Note start: retain note/velocity, repeat the unison allocation count,
    // then configure each selected voice. Parameter-fill callees stay H8.
    // Start one allocated voice, including velocity modulation, portamento
    // and retrigger flags; preserve the original parameter-engine call order.
    N(0x902a,4,f.call(0xa338))
    N(0x902e,8,f.move(r[0].rl,h.read8(r[3].er+6)))
    N(0x9036,4,f.branch(bool(h.ccr&8),0x92be))
    N(0x903a,4,f.call(0xa1b0))
    N(0x903e,6,f.move(r[0].rl,h.read8(0x1752a)))
    N(0x9044,2,h.sub8(r[0].rl,0x1))
    N(0x9046,2,f.branch(!(h.ccr&5),0x9084))
    N(0x9048,4,f.move(r[4].r,0x2c))
    N(0x904c,4,f.call(0x77dc))
    N(0x9050,4,f.call(0x7de4))
    N(0x9054,8,f.move(r[1].rh,h.read8(r[6].er+79)))
    N(0x905c,2,r[1].rh|=0xffffffc0;value(h,r[1].rh))
    N(0x905e,8,f.move(r[1].rl,h.read8(r[6].er+147)))
    N(0x9066,2,f.shift(r[1].rl,1,true))
    N(0x9068,2,f.move(r[1].e,r[1].r))
    N(0x906a,2,f.move(r[1].rh,0xffffffbf))
    N(0x906c,8,f.move(r[0].rl,h.read8(r[6].er+4)))
    N(0x9074,2,r[0].rl=h.add8(r[0].rl,0xffffffc0))
    N(0x9076,2,f.branch(bool(h.ccr&4),0x907a))
    N(0x9078,2,r[1].rh|=0x40;value(h,r[1].rh))
    N(0x907a,4,f.move(r[4].r,0x2b))
    N(0x907e,4,f.call(0x785a))
    N(0x9082,2,f.ret())
    N(0x927e,8,f.move(r[1].rh,h.read8(r[6].er+79)))
    N(0x9286,2,r[1].rh&=0x3f;value(h,r[1].rh))
    N(0x9288,8,f.move(r[1].rl,h.read8(r[6].er+78)))
    N(0x9290,2,r[1].rl&=0x3;value(h,r[1].rl))
    N(0x9292,2,f.rotateRight(r[1].rl,2))
    N(0x9294,2,r[1].rh|=r[1].rl;value(h,r[1].rh))
    N(0x9296,2,f.move(r[1].rl,0x0))
    N(0x9298,2,f.move(r[1].e,r[1].r))
    N(0x929a,8,f.move(r[1].rh,h.read8(r[6].er+80)))
    N(0x92a2,2,r[1].rh&=0x7;value(h,r[1].rh))
    N(0x92a4,2,r[1].rh|=0xffffffb8;value(h,r[1].rh))
    N(0x92a6,8,f.move(r[0].rl,h.read8(r[6].er+4)))
    N(0x92ae,2,r[0].rl=h.add8(r[0].rl,0xffffffc0))
    N(0x92b0,2,f.branch(bool(h.ccr&4),0x92b4))
    N(0x92b2,2,r[1].rh|=0x40;value(h,r[1].rh))
    N(0x92b4,4,f.move(r[4].r,0x2b))
    N(0x92b8,4,f.call(0x785a))
    N(0x92bc,2,f.ret())
    N(0x92be,2,f.ret())
    // Release matching pitch/part voices, retaining pedal-held slots.
    // 0xa338..0xa656: verified interruptible controller boundaries.
    // Choose a voice according to mono/auto/poly/retrigger mode. Keep
    // slot age comparisons, priority and steal/reuse behavior exact.
    // Populate the selected slot, apply transposition and velocity curve,
    // snapshot part controls, then read the DSP pitch for glide distance.
    // Velocity curves: repeated squaring, inversion and threshold curves;
    // curve thresholds are read from the user-provided ROM, never embedded.
    // 0xa67e..0xa69e: verified interruptible controller boundaries.
    // Copy the selected part control mirror for this voice lifecycle.
    default:return false;
    }
#undef N
}
}
