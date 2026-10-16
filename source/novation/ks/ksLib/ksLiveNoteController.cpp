#include "ksLiveNoteController.h"
#include "ksControllerBoundary.h"
#include <stdexcept>

namespace ks {
using namespace controllerDetail;
void LiveNoteController::verify(const ControllerCpu& h){
    uint64_t hash=14695981039346656037ull;
    for(auto range:{std::pair<unsigned,unsigned>{0x2efc,0x31dc},{0x33b8,0x349c},{0x352c,0x358a},
                   {0x3642,0x36d2},{0x4000,0x4016},{0x9374,0x9484},{0xa656,0xa67e}})
        for(unsigned n=range.first;n<range.second;++n){hash^=h.memory[n];hash*=1099511628211ull;}
    if(hash!=0x49a3c988f9f72c6cull)throw std::runtime_error("Native KS note/sustain handlers require verified OS 2.1.02");
}
bool LiveNoteController::step(ControllerCpu& h){
    auto& r=h.regs;
    const unsigned pc=h.getPC();
    if(KeyPriorityCalls::candidate(pc)){if(!keyPriorityCalls.step(h))return false;++steps;return true;}
    if(NoteOffStateCalls::candidate(pc)){if(!noteOffStateCalls.step(h))return false;++steps;return true;}
    if(VoiceReleaseCalls::candidate(pc)){if(!releaseCalls.step(h))return false;++steps;if(pc==0x9400)++pedalReleaseCalls;return true;}
    if(NoteOnStateCalls::candidate(pc)){if(!noteOnStateCalls.step(h))return false;++steps;return true;}
    if(KeyActivityCalls::candidate(pc)){if(!keyActivityCalls.step(h))return false;++steps;return true;}
    if(KeyResetCalls::candidate(pc)){if(!keyResetCalls.step(h))return false;++steps;return true;}
    if(KeySearchCalls::candidate(pc)){if(!keySearchCalls.step(h))return false;++steps;return true;}
    if(pc<0xa67e && PartControlCalls::candidate(pc)){if(!partControlCalls.step(h))return false;++steps;return true;}
    // Counts are actual handler execution, independently of decoded MIDI counts.
    if(pc==0x3426)++noteOnCalls;
    if(pc==0x33b8)++noteOffCalls;
    if(pc==0x3530)++pedalCalls;
    if(pc==0x9400)++pedalReleaseCalls;
#define N(P,L,...) case P:{Boundary f(h,P,L); __VA_ARGS__;h.pc=h.makepc(f.next);++steps;return true;}
    switch(pc){
    // Note On: append the note/velocity pair, retain velocity by key, set the
    // physical-down and sustain-held bitmaps for the selected part.
    // Note Off: clear physical-down immediately; defer held-key removal while
    // the pedal is down, otherwise remove the key and update mono priority.
    // Compact the ordered key list, preserving the original duplicate policy.
    // All-notes-off / held-key reset; keep velocity memory and program intact.
    // Mono replacement note: search adjacent held keys and restore velocity.
    // Part Note Off/On: keyboard range, zero velocity, mono/poly and retrigger
    // decisions. DSP voice synthesis is still delegated to its original ABI.
    N(0x33b8,2,f.move(r[5].rl,h.read8(r[4].er)))
    N(0x33ba,4,f.call(0xa66a))
    N(0x33be,8,f.move(r[1].rl,h.read8(r[0].er+12)))
    N(0x33c6,8,f.move(r[1].rh,h.read8(r[0].er+13)))
    N(0x33ce,2,h.sub8(r[5].rl,r[1].rl))
    N(0x33d0,2,f.branch(bool(h.ccr&1),0x3424))
    N(0x33d2,2,h.sub8(r[5].rl,r[1].rh))
    N(0x33d4,2,f.branch(!(h.ccr&5),0x3424))
    N(0x33d6,4,f.call(0x2f92))
    N(0x33da,4,f.call(0x4000))
    N(0x33de,8,f.move(r[1].rl,h.read8(r[0].er+327)))
    N(0x33e6,2,flag(h,4,(uint8_t(r[1].rl)&(1u<<0x2))==0))
    N(0x33e8,2,f.branch(bool(h.ccr&4),0x33f0))
    N(0x33ea,2,r[1].rl&=0x60;value(h,r[1].rl))
    N(0x33ec,2,h.sub8(r[1].rl,0x60))
    N(0x33ee,2,f.branch(!(h.ccr&4),0x3424))
    N(0x33f0,2,f.push(r[5].r))
    N(0x33f2,4,f.call(0x4000))
    N(0x33f6,8,f.move(r[1].rl,h.read8(r[0].er+313)))
    N(0x33fe,2,h.sub8(r[1].rl,0x1))
    N(0x3400,2,f.branch(!(h.ccr&5),0x341e))
    N(0x3402,8,f.move(r[0].rl,h.read8(r[0].er+314)))
    N(0x340a,2,h.sub8(r[5].rl,r[0].rl))
    N(0x340c,2,f.branch(!(h.ccr&4),0x341e))
    N(0x340e,4,f.call(0x3118))
    N(0x3412,2,f.move(r[5].rl,r[5].rl))
    N(0x3414,2,f.branch(bool(h.ccr&8),0x341e))
    N(0x3416,4,f.call(0x3642))
    N(0x341a,2,f.pop(r[5].r))
    N(0x341c,2,f.ret())
    N(0x341e,2,f.pop(r[5].r))
    N(0x3420,4,f.call(0x92c0))
    N(0x3424,2,f.ret())
    N(0x3426,6,f.move(r[0].rl,h.read8(0xffec14)))
    N(0x342c,2,f.branch(bool(h.ccr&4),0x3440))
    N(0x342e,6,f.move(r[0].rl,h.read8(0x17526)))
    N(0x3434,6,f.move(r[0].rh,h.read8(0x16798)))
    N(0x343a,2,h.sub8(r[0].rh,r[0].rl))
    N(0x343c,2,f.branch(bool(h.ccr&4),0x3448))
    N(0x343e,2,f.branch(true,0x3456))
    N(0x3440,6,f.move(r[0].rl,h.read8(0x15c15)))
    N(0x3446,2,f.branch(bool(h.ccr&4),0x3456))
    N(0x3448,2,f.move(r[6].rl,h.read8(r[4].er)))
    N(0x344a,4,f.push(r[4].er))
    N(0x344e,4,f.call(0x4d90))
    N(0x3452,4,f.pop(r[4].er))
    N(0x3456,2,f.move(r[5].rl,h.read8(r[4].er)))
    N(0x3458,4,f.call(0xa66a))
    N(0x345c,8,f.move(r[1].rl,h.read8(r[0].er+12)))
    N(0x3464,8,f.move(r[1].rh,h.read8(r[0].er+13)))
    N(0x346c,2,h.sub8(r[5].rl,r[1].rl))
    N(0x346e,2,f.branch(bool(h.ccr&1),0x349a))
    N(0x3470,2,h.sub8(r[5].rl,r[1].rh))
    N(0x3472,2,f.branch(!(h.ccr&5),0x349a))
    N(0x3474,4,f.move(r[5].rh,h.read8(r[4].er+1)))
    N(0x3478,4,f.branch(bool(h.ccr&4),0x33d6))
    N(0x347c,4,f.call(0x2efc))
    N(0x3480,4,f.call(0x4000))
    N(0x3484,8,f.move(r[1].rl,h.read8(r[0].er+327)))
    N(0x348c,2,flag(h,4,(uint8_t(r[1].rl)&(1u<<0x2))==0))
    N(0x348e,2,f.branch(bool(h.ccr&4),0x3496))
    N(0x3490,2,r[1].rl&=0x60;value(h,r[1].rl))
    N(0x3492,2,h.sub8(r[1].rl,0x60))
    N(0x3494,2,f.branch(!(h.ccr&4),0x349a))
    N(0x3496,4,f.call(0x3642))
    N(0x349a,2,f.ret())
    // CC64: values below 64 release, 64..127 hold; repeat values keep state.
    N(0x352c,2,h.sub8(r[0].rh,0x40))
    N(0x352e,2,f.branch(!(h.ccr&4),0x358a))
    N(0x3530,2,f.move(r[3].rh,0x0))
    N(0x3532,2,h.sub8(r[0].rl,0x40))
    N(0x3534,2,f.branch(bool(h.ccr&1),0x3538))
    N(0x3536,2,r[3].rh|=(1u<<0x4))
    N(0x3538,4,f.call(0x4000))
    N(0x353c,8,f.move(r[3].rl,h.read8(r[0].er+309)))
    N(0x3544,8,f.store8(r[0].er+309,r[3].rh))
    N(0x354c,8,f.store8(r[0].er+310,r[3].rh))
    N(0x3554,4,f.push(r[0].er))
    N(0x3558,2,f.move(r[0].rl,r[2].rl))
    N(0x355a,4,f.call(0xa670))
    N(0x355e,10,f.move(r[1].er,h.read32(r[0].er+0)))
    N(0x3568,4,f.pop(r[0].er))
    N(0x356c,8,f.move(r[2].rl,h.read8(r[1].er+88)))
    N(0x3574,2,flag(h,4,(uint8_t(r[2].rl)&(1u<<0x2))==0))
    N(0x3576,2,f.branch(!(h.ccr&4),0x3588))
    N(0x3578,2,r[3].rh^=r[3].rl;value(h,r[3].rh))
    N(0x357a,2,f.branch(bool(h.ccr&4),0x3586))
    N(0x357c,2,r[3].rl&=r[3].rh;value(h,r[3].rl))
    N(0x357e,2,flag(h,4,(uint8_t(r[3].rl)&(1u<<0x4))==0))
    N(0x3580,2,f.branch(bool(h.ccr&4),0x3586))
    N(0x3582,4,f.call(0x9400))
    N(0x3586,2,f.ret())
    N(0x3588,2,f.ret())
    // Select generated/sample-wave program and request the existing voice path.
    N(0x3642,2,r[0].er=h.sub32(r[0].er,r[0].er))
    N(0x3644,6,f.move(r[0].rl,h.read8(0x17526)))
    N(0x364a,2,f.shift(r[0].rl,1,true))
    N(0x364c,8,f.move(r[0].r,h.read16(r[0].er+92078)))
    N(0x3654,2,h.sub8(r[0].rh,0x4))
    N(0x3656,2,f.branch(bool(h.ccr&5),0x36bc))
    N(0x3658,2,h.sub8(r[5].rl,0x24))
    N(0x365a,2,f.branch(bool(h.ccr&1),0x36d0))
    N(0x365c,2,h.sub8(r[5].rl,0x54))
    N(0x365e,2,f.branch(!(h.ccr&5),0x36d0))
    N(0x3660,2,f.move(r[3].rl,r[5].rl))
    N(0x3662,2,r[3].rl=h.add8(r[3].rl,0xffffffdc))
    N(0x3664,2,h.sub8(r[3].rl,r[0].rl))
    N(0x3666,2,f.branch(bool(h.ccr&4),0x36bc))
    N(0x3668,2,r[1].r=h.sub16(r[1].r,r[1].r))
    N(0x366a,2,f.move(r[1].rl,r[3].rl))
    N(0x366c,4,f.move(r[1].e,0x100))
    N(0x3670,2,r[1].er=uint16_t(r[1].er)*uint32_t(uint16_t(r[1].e));h.clockI(20))
    N(0x3672,2,f.move(r[0].rl,r[0].rh))
    N(0x3674,2,r[0].rl=h.add8(r[0].rl,0xfffffffb))
    N(0x3676,2,r[0].r&=0xff;value(h,r[0].r))
    N(0x3678,4,f.move(r[0].e,0x3200))
    N(0x367c,2,r[0].er=uint16_t(r[0].er)*uint32_t(uint16_t(r[0].e));h.clockI(20))
    N(0x367e,2,r[1].er=h.add32(r[1].er,r[0].er))
    N(0x3680,6,r[1].er=h.add32(r[1].er,0x839000))
    N(0x3686,4,f.call(0xa66a))
    N(0x368a,10,f.move(r[2].er,h.read32(r[0].er+0)))
    N(0x3694,10,f.store32(r[0].er+0,r[1].er))
    N(0x369e,4,f.push(r[2].er))
    N(0x36a2,4,f.push(r[0].er))
    N(0x36a6,2,f.call(0x36bc))
    N(0x36a8,4,f.pop(r[0].er))
    N(0x36ac,4,f.pop(r[2].er))
    N(0x36b0,10,f.store32(r[0].er+0,r[2].er))
    N(0x36ba,2,f.ret())
    N(0x36bc,4,f.call(0x4000))
    N(0x36c0,8,f.store8(r[0].er+314,r[5].rl))
    N(0x36c8,4,f.call(0x8fcc))
    N(0x36cc,4,f.call(0xa00e))
    N(0x36d0,2,f.ret())
    // Part release/all-notes-off walks the existing voice slots. The low-level
    // DSP release upload remains original, with the same call clock/ordering.
    // Pedal-up releases only active, matching-part voices marked as held.
    default:return false;
    }
#undef N
}
}
