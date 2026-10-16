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
