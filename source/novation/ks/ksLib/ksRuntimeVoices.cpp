#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"

namespace ks {
// Periodic voice/DSP feedback, control updates, allocation and note/pedal release.
// Fixed native control flow for the verified KS revision. Private ROM data
// remains external; instruction fetch reads account for original bus clocks.
bool RuntimeController::voices(ControllerCpu& h){
    shared.voiceActions.globalFinishScalarCalls.retireReturns(h);
    shared.voiceActions.globalTimeTailCalls.retireReturns(h);
    shared.voiceActions.globalTailScalarCalls.retireReturns(h);
    shared.voiceActions.globalPairCalls.retireReturns(h);
    shared.voiceActions.globalClampedTimeCalls.retireReturns(h);
    shared.voiceActions.globalMatrixCalls.retireReturns(h);
    shared.voiceActions.globalMixCalls.retireReturns(h);
    shared.voiceActions.globalSetupCalls.retireReturns(h);
    shared.voiceActions.globalScalarCalls.retireReturns(h);
    shared.voiceActions.globalTimeCalls.retireReturns(h);
    shared.voiceActions.globalShapeCalls.retireReturns(h);
    shared.voiceActions.globalRangeCalls.retireReturns(h);
    if(GlobalMixCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalMixCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalTableCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalTableCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(SquareRootCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.squareRootCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalMatrixCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalMatrixCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalClampedTimeCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalClampedTimeCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalPairCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalPairCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalTailScalarCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalTailScalarCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalTimeTailCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalTimeTailCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalFinishScalarCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalFinishScalarCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalFinalCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalFinalCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(VoiceSweepCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.sweepCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(VoiceRefreshCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.refreshCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalSetupCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalSetupCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalScalarCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalScalarCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalTimeCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalTimeCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalShapeCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalShapeCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalRangeCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalRangeCalls.step(h)){++shared.voiceActions.steps;return true;}
    if(GlobalEffectCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.globalEffectCalls.step(h))return true;
    if(PartModulationCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.partModulationCalls.step(h))return true;
    if(PartRouteCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.partRouteCalls.step(h))return true;
    if(VoiceFeedbackCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.voiceFeedbackCalls.step(h))return true;
    if(PartEnableCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.partEnableCalls.step(h))return true;
    if(PartSignalCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.partSignalCalls.step(h))return true;
    if(PeriodicCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.periodicCalls.step(h))return true;
    if(SyncValueCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.syncValueCalls.step(h))return true;
    if(PartRefreshCalls::candidate(unsigned(h.getPC()))&&shared.voiceActions.partRefreshCalls.step(h))return true;
    return shared.noteActions.step(h)||shared.voiceActions.step(h);
}
}
