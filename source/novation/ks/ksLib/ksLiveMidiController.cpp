#include "ksLiveMidiController.h"
#include <stdexcept>

namespace ks {
namespace {
bool interruptReady(const ControllerCpu& h){
    if(!(h.pending_irqs||h.pending_irqs_high))return false;
    if(!h.interruptMode2)return (h.pending_irqs!=0)&&(!(h.ccr&128)||!(h.ccr&16));
    for(unsigned n=0;n<128;++n)if(((n<64?h.pending_irqs:h.pending_irqs_high)>>(n&63))&1)
        if(h.interruptPriority[n]>(h.exr&7))return true;
    return false;
}
}
LiveMidiController::LiveMidiController(const ControllerCpu& h){
    // Reject unverified parser/table revisions. No firmware bytes are embedded.
    uint64_t hash=14695981039346656037ull;
    for(auto range:{std::pair<unsigned,unsigned>{0x2de8,0x2efc},{0xf250,0xf2da}})
        for(unsigned n=range.first;n<range.second;++n){hash^=h.memory[n];hash*=1099511628211ull;}
    if(hash!=0x883c476e3df41f22ull)throw std::runtime_error("Native KS MIDI decoder requires verified OS 2.1.02");
    LiveNoteController::verify(h);
    LiveVoiceController::verify(h);
}
bool LiveMidiController::step(ControllerCpu& h){
    const unsigned pc=h.getPC();
    if(interruptReady(h))return false;
    if(MidiPartInitCalls::candidate(pc))return midiPartInitCalls.step(h);
    if(MidiRouteCalls::candidate(pc))return midiRouteCalls.step(h);
    if(LiveVoiceController::candidate(pc))return voiceActions.step(h);
    if(LiveNoteController::candidate(pc))return noteActions.step(h);
    if(MidiControlCalls::candidate(pc))return midiControlCalls.step(h);
    if(MidiAuxCalls::candidate(pc))return midiAuxCalls.step(h);
    if(!MidiDecodeCalls::candidate(pc)||!midiDecodeCalls.step(h))return false;
    ++nativeSteps;if(pc==0x2de8)++bytes;
    if(pc==0x2ecc){
        ++messages;const auto kind=h.memory[0xa15c1b]&0xf0;
        if(kind==0xb0)++controls;
        else if(kind==0x80||(kind==0x90&&!h.memory[0xa15c20]))++noteOff;
        else if(kind==0x90)++noteOn;
    }
    return true;
}
}
