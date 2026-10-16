#pragma once
#include "ksControllerState.h"
#include "ksPartControlCalls.h"
#include "ksVelocityCurveCalls.h"
#include "ksVoiceSelectionCalls.h"
#include "ksVoicePopulationCalls.h"
#include "ksUnisonCalls.h"
#include "ksVoiceTrackingCalls.h"
#include "ksVelocityModulationCalls.h"
#include "ksGlideCalls.h"
#include "ksVoiceStartCalls.h"
#include "ksDspWordCalls.h"
#include "ksDspAddressCalls.h"
#include "ksDspStartupCalls.h"
#include "ksDspRoutingCalls.h"
#include "ksOscillatorModulationCalls.h"
#include "ksGlobalModulationCalls.h"
#include "ksVoiceSweepCalls.h"
#include "ksOscillatorPitchCalls.h"
#include "ksParameterPitchCalls.h"
#include "ksParameterModulationCalls.h"
#include "ksVoiceRefreshCalls.h"
#include "ksGlobalSetupCalls.h"
#include "ksParameterScalarCalls.h"
#include "ksGlobalScalarCalls.h"
#include "ksGlobalTimeCalls.h"
#include "ksParameterTimeCalls.h"
#include "ksParameterCurveCalls.h"
#include "ksParameterVoiceScaleCalls.h"
#include "ksGlobalShapeCalls.h"
#include "ksGlobalRangeCalls.h"
#include "ksParameterHeaderCalls.h"
#include "ksParameterSignedSquareCalls.h"
#include "ksParameterFilterCalls.h"
#include "ksParameterVolumeCalls.h"
#include "ksParameterGateCalls.h"
#include "ksParameterSupportCalls.h"
#include "ksParameterLfoCalls.h"
#include "ksGlobalMixCalls.h"
#include "ksGlobalTableCalls.h"
#include "ksSquareRootCalls.h"
#include "ksGlobalMatrixCalls.h"
#include "ksGlobalClampedTimeCalls.h"
#include "ksGlobalPairCalls.h"
#include "ksGlobalTailScalarCalls.h"
#include "ksGlobalTimeTailCalls.h"
#include "ksGlobalFinishScalarCalls.h"
#include "ksGlobalFinalCalls.h"
#include <cstdint>

namespace ks {
class ControllerCpu;
// Native voice lifecycle and DSP host transport. Parameter calculators and
// periodic voice servicing retain the compatibility ABI during migration.
class LiveVoiceController final {
public:
    static constexpr bool candidate(unsigned pc){
        return (pc>=0x2416 && pc<0x25fe)||(pc>=0x77dc && pc<0x7872)||
            (pc>=0x8fc0 && pc<0x92c0)||(pc>=0xa338 && pc<0xa656)||
            (pc>=0xa67e && pc<0xa69e)||(pc>=0xa1b0 && pc<0xa338);
    }
    static void verify(const ControllerCpu&);
    bool step(ControllerCpu&);
    void resetMetrics(){globalFinalCalls.resetMetrics();globalFinishScalarCalls.resetMetrics();globalTimeTailCalls.resetMetrics();globalTailScalarCalls.resetMetrics();globalPairCalls.resetMetrics();globalClampedTimeCalls.resetMetrics();globalMatrixCalls.resetMetrics();squareRootCalls.resetMetrics();globalTableCalls.resetMetrics();globalMixCalls.resetMetrics();parameterLfoCalls.resetMetrics();parameterSupportCalls.resetMetrics();parameterGateCalls.resetMetrics();parameterVolumeCalls.resetMetrics();parameterFilterCalls.resetMetrics();parameterSignedSquareCalls.resetMetrics();parameterHeaderCalls.resetMetrics();globalRangeCalls.resetMetrics();globalShapeCalls.resetMetrics();parameterVoiceScaleCalls.resetMetrics();parameterCurveCalls.resetMetrics();parameterTimeCalls.resetMetrics();globalTimeCalls.resetMetrics();globalScalarCalls.resetMetrics();parameterScalarCalls.resetMetrics();globalSetupCalls.resetMetrics();refreshCalls.resetMetrics();parameterModulationCalls.resetMetrics();parameterPitchCalls.resetMetrics();oscillatorPitchCalls.resetMetrics();sweepCalls.resetMetrics();globalModulationCalls.resetMetrics();oscillatorModulationCalls.resetMetrics();steps=allocationCalls=startCalls=releaseCalls=transportSteps=0;wordCalls.resetMetrics();addressCalls.resetMetrics();selectCalls.resetMetrics();routingCalls.resetMetrics();partControlCalls.resetMetrics();velocityCurveCalls.resetMetrics();selectionCalls.resetMetrics();populationCalls.resetMetrics();unisonCalls.resetMetrics();trackingCalls.resetMetrics();modulationCalls.resetMetrics();glideCalls.resetMetrics();startCallsNative.resetMetrics();}
    void discardTransientCalls(){globalFinalCalls.discardCalls();globalFinishScalarCalls.discardCalls();globalTimeTailCalls.discardCalls();globalTailScalarCalls.discardCalls();globalPairCalls.discardCalls();globalClampedTimeCalls.discardCalls();globalMatrixCalls.discardCalls();squareRootCalls.discardCalls();globalTableCalls.discardCalls();globalMixCalls.discardCalls();parameterLfoCalls.discardCalls();parameterSupportCalls.discardCalls();parameterGateCalls.discardCalls();parameterVolumeCalls.discardCalls();parameterFilterCalls.discardCalls();parameterSignedSquareCalls.discardCalls();parameterHeaderCalls.discardCalls();globalRangeCalls.discardCalls();globalShapeCalls.discardCalls();parameterVoiceScaleCalls.discardCalls();parameterCurveCalls.discardCalls();parameterTimeCalls.discardCalls();globalTimeCalls.discardCalls();globalScalarCalls.discardCalls();parameterScalarCalls.discardCalls();globalSetupCalls.discardCalls();refreshCalls.discardCalls();parameterModulationCalls.discardCalls();parameterPitchCalls.discardCalls();oscillatorPitchCalls.discardCalls();sweepCalls.discardCalls();globalModulationCalls.discardCalls();oscillatorModulationCalls.discardCalls();wordCalls.discardCalls();addressCalls.discardCalls();selectCalls.discardCalls();routingCalls.discardCalls();partControlCalls.discardCalls();velocityCurveCalls.discardCalls();selectionCalls.discardCalls();populationCalls.discardCalls();unisonCalls.discardCalls();trackingCalls.discardCalls();modulationCalls.discardCalls();glideCalls.discardCalls();startCallsNative.discardCalls();}
    DspWordCalls wordCalls;
    DspAddressCalls addressCalls;
    DspSelectCalls selectCalls;
    DspRoutingCalls routingCalls;
    PartControlCalls partControlCalls;
    VelocityCurveCalls velocityCurveCalls;
    VoiceSelectionCalls selectionCalls;
    VoicePopulationCalls populationCalls;
    UnisonCalls unisonCalls;
    VoiceTrackingCalls trackingCalls;
    VelocityModulationCalls modulationCalls;
    GlideCalls glideCalls;
    VoiceStartCalls startCallsNative;
    OscillatorModulationCalls oscillatorModulationCalls;
    GlobalModulationCalls globalModulationCalls;
    VoiceSweepCalls sweepCalls;
    OscillatorPitchCalls oscillatorPitchCalls;
    ParameterPitchCalls parameterPitchCalls;
    ParameterModulationCalls parameterModulationCalls;
    VoiceRefreshCalls refreshCalls;
    GlobalSetupCalls globalSetupCalls;
    ParameterScalarCalls parameterScalarCalls;
    GlobalScalarCalls globalScalarCalls;
    GlobalTimeCalls globalTimeCalls;
    ParameterTimeCalls parameterTimeCalls;
    ParameterCurveCalls parameterCurveCalls;
    ParameterVoiceScaleCalls parameterVoiceScaleCalls;
    GlobalShapeCalls globalShapeCalls;
    GlobalRangeCalls globalRangeCalls;
    ParameterHeaderCalls parameterHeaderCalls;
    ParameterSignedSquareCalls parameterSignedSquareCalls;
    ParameterFilterCalls parameterFilterCalls;
    ParameterVolumeCalls parameterVolumeCalls;
    ParameterGateCalls parameterGateCalls;
    ParameterSupportCalls parameterSupportCalls;
    ParameterLfoCalls parameterLfoCalls;
    GlobalMixCalls globalMixCalls;
    GlobalTableCalls globalTableCalls;
    SquareRootCalls squareRootCalls;
    GlobalMatrixCalls globalMatrixCalls;
    GlobalClampedTimeCalls globalClampedTimeCalls;
    GlobalPairCalls globalPairCalls;
    GlobalTailScalarCalls globalTailScalarCalls;
    GlobalTimeTailCalls globalTimeTailCalls;
    GlobalFinishScalarCalls globalFinishScalarCalls;
    GlobalFinalCalls globalFinalCalls;
    uint64_t steps=0,allocationCalls=0,startCalls=0,releaseCalls=0,transportSteps=0;
};
}
