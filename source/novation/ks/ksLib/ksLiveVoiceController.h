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
    void resetMetrics(){steps=allocationCalls=startCalls=releaseCalls=transportSteps=0;wordCalls.resetMetrics();addressCalls.resetMetrics();selectCalls.resetMetrics();routingCalls.resetMetrics();partControlCalls.resetMetrics();velocityCurveCalls.resetMetrics();selectionCalls.resetMetrics();populationCalls.resetMetrics();unisonCalls.resetMetrics();trackingCalls.resetMetrics();modulationCalls.resetMetrics();glideCalls.resetMetrics();startCallsNative.resetMetrics();}
    void discardTransientCalls(){wordCalls.discardCalls();addressCalls.discardCalls();selectCalls.discardCalls();routingCalls.discardCalls();partControlCalls.discardCalls();velocityCurveCalls.discardCalls();selectionCalls.discardCalls();populationCalls.discardCalls();unisonCalls.discardCalls();trackingCalls.discardCalls();modulationCalls.discardCalls();glideCalls.discardCalls();startCallsNative.discardCalls();}
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
    uint64_t steps=0,allocationCalls=0,startCalls=0,releaseCalls=0,transportSteps=0;
};
}
