#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"

namespace ks {
// All channel/system MIDI, SysEx dispatch, key/CC and arpeggiator scheduling.
// Fixed native control flow for the verified KS revision. Private ROM data
// remains external; instruction fetch reads account for original bus clocks.
bool RuntimeController::midi(ControllerCpu& h){
    if(InternalMidiServiceCalls::candidate(unsigned(h.getPC()))&&shared.internalMidiServiceCalls.step(h))return true;
    if(MidiDecodeCalls::candidate(unsigned(h.getPC()))&&shared.midiDecodeCalls.step(h))return true;
    if(MidiControlCalls::candidate(unsigned(h.getPC()))&&shared.midiControlCalls.step(h))return true;
    if(MidiAuxCalls::candidate(unsigned(h.getPC()))&&shared.midiAuxCalls.step(h))return true;
    if(MidiPartInitCalls::candidate(unsigned(h.getPC()))&&shared.midiPartInitCalls.step(h))return true;
    if(MidiRouteCalls::candidate(unsigned(h.getPC()))&&shared.midiRouteCalls.step(h))return true;
    if(ArpPrimitiveCalls::candidate(unsigned(h.getPC()))&&shared.arpPrimitiveCalls.step(h))return true;
    if(ArpScheduleCalls::candidate(unsigned(h.getPC()))&&shared.arpScheduleCalls.step(h))return true;
    if(ArpMainCalls::candidate(unsigned(h.getPC()))&&shared.arpMainCalls.step(h))return true;
    if(ArpEventCalls::candidate(unsigned(h.getPC()))&&shared.arpEventCalls.step(h))return true;
    if(ArpSelectCalls::candidate(unsigned(h.getPC()))&&shared.arpSelectCalls.step(h))return true;
    return shared.noteActions.step(h);
}
}
