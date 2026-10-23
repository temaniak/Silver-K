#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"

namespace ks {
// Parameter scaling, internal MIDI/arp, compatibility and performance-edit helpers.
// Fixed native control flow for the verified KS revision. Private ROM data
// remains external; instruction fetch reads account for original bus clocks.
bool RuntimeController::extended(ControllerCpu& h){
    if(ParameterScaleCalls::candidate(unsigned(h.getPC())))return shared.parameterScaleCalls.step(h);
    if(VoiceTableInitCalls::candidate(unsigned(h.getPC()))&&shared.voiceTableInitCalls.step(h))return true;
    if(InternalMidiCalls::candidate(unsigned(h.getPC()))&&shared.internalMidiCalls.step(h))return true;
    if(InternalNoteCalls::candidate(unsigned(h.getPC()))&&shared.internalNoteCalls.step(h))return true;
    if(SysExFrameCalls::candidate(unsigned(h.getPC()))&&shared.sysExFrameCalls.step(h))return true;
    if(PatchExpandCalls::candidate(unsigned(h.getPC()))&&shared.patchExpandCalls.step(h))return true;
    if(SysExActionCalls::candidate(unsigned(h.getPC()))&&shared.sysExActionCalls.step(h))return true;
    if(LegacySysExCalls::candidate(unsigned(h.getPC()))&&shared.legacySysExCalls.step(h))return true;
    if(SysExDumpCalls::candidate(unsigned(h.getPC()))&&shared.sysExDumpCalls.step(h))return true;
    if(MidiFileTimeCalls::candidate(unsigned(h.getPC()))&&shared.midiFileTimeCalls.step(h))return true;
    if(MidiFileEventCalls::candidate(unsigned(h.getPC()))&&shared.midiFileEventCalls.step(h))return true;
    if(MonoArpCalls::candidate(unsigned(h.getPC()))&&shared.monoArpCalls.step(h))return true;
    if(MidiFileTransportCalls::candidate(unsigned(h.getPC()))&&shared.midiFileTransportCalls.step(h))return true;
    if(EditAddressCalls::candidate(unsigned(h.getPC()))&&shared.editAddressCalls.step(h))return true;
    if(EditSceneCalls::candidate(unsigned(h.getPC()))&&shared.editSceneCalls.step(h))return true;
    if(EditDecisionCalls::candidate(unsigned(h.getPC()))&&shared.editDecisionCalls.step(h))return true;
    return false;
}
}
