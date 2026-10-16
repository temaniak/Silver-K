#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"

namespace ks {
// Voice parameter conversion and DSP uploads: oscillator, envelopes, filter, LFO and effects.
// Fixed native control flow for the verified KS revision. Private ROM data
// remains external; instruction fetch reads account for original bus clocks.
bool RuntimeController::parameters(ControllerCpu& h){
    shared.voiceActions.glideCalls.retireReturns(h);
    shared.voiceActions.modulationCalls.retireReturns(h);
    shared.voiceActions.parameterLfoCalls.retireReturns(h);
    shared.voiceActions.parameterSupportCalls.retireReturns(h);
    shared.voiceActions.parameterGateCalls.retireReturns(h);
    shared.voiceActions.parameterVolumeCalls.retireReturns(h);
    shared.voiceActions.parameterFilterCalls.retireReturns(h);
    shared.voiceActions.parameterSignedSquareCalls.retireReturns(h);
    shared.voiceActions.parameterHeaderCalls.retireReturns(h);
    shared.voiceActions.parameterTimeCalls.retireReturns(h);
    shared.voiceActions.parameterCurveCalls.retireReturns(h);
    shared.voiceActions.parameterVoiceScaleCalls.retireReturns(h);
    shared.voiceActions.parameterPitchCalls.retireReturns(h);
    shared.voiceActions.parameterModulationCalls.retireReturns(h);
    shared.voiceActions.parameterScalarCalls.retireReturns(h);
    if(ParameterHeaderCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterHeaderCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterSignedSquareCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterSignedSquareCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterFilterCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterFilterCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterVolumeCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterVolumeCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterGateCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterGateCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterSupportCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterSupportCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterLfoCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterLfoCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(VelocityModulationCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.modulationCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlideCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.glideCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(OscillatorModulationCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.oscillatorModulationCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalModulationCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalModulationCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(OscillatorPitchCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.oscillatorPitchCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterPitchCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterPitchCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterModulationCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterModulationCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterScalarCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterScalarCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterTimeCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterTimeCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterCurveCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterCurveCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(ParameterVoiceScaleCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.parameterVoiceScaleCalls.step(h)){++shared.voiceActions.steps;return true;}
    return shared.voiceActions.step(h);
}
}
