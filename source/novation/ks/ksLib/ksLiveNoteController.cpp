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
    if(PartNoteCalls::candidate(pc)){if(!partNoteCalls.step(h))return false;if(pc==0x3426)++noteOnCalls;if(pc==0x33b8)++noteOffCalls;++steps;return true;}
    if(PedalCalls::candidate(pc)){if(!pedalNativeCalls.step(h))return false;if(pc==0x3530)++pedalCalls;++steps;return true;}
    if(NoteProgramCalls::candidate(pc)){if(!noteProgramCalls.step(h))return false;++steps;return true;}
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
    // CC64: values below 64 release, 64..127 hold; repeat values keep state.
    // Select generated/sample-wave program and request the existing voice path.
    // Part release/all-notes-off walks the existing voice slots. The low-level
    // DSP release upload remains original, with the same call clock/ordering.
    // Pedal-up releases only active, matching-part voices marked as held.
    default:return false;
    }
#undef N
}
}
