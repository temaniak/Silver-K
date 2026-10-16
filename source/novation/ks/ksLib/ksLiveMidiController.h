#pragma once
#include "ksControllerState.h"
#include <cstdint>
#include <cstddef>
#include <utility>
#include "ksLiveNoteController.h"
#include "ksLiveVoiceController.h"
#include "ksParameterScaleCalls.h"
#include "ksDspFaultCalls.h"
#include "ksDspBootStreamCalls.h"
#include "ksDspBootSetupCalls.h"
#include "ksDspStartupCalls.h"
#include "ksScheduledTimerCalls.h"
#include "ksServiceCycleCalls.h"
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace ks {
// Live C++ byte decoder and note/key/sustain handlers for OS 2.1.02.
// Retains the firmware ABI, UART timing and interrupt boundaries.
// Parameter calculators, periodic servicing, clock/arp and state loaders
// remain downstream. Voice lifecycle and DSP transport also run natively.
// This is a hybrid, NOT yet a controller that can run with H8 removed.
class LiveMidiController final {
public:
    explicit LiveMidiController(const ControllerCpu&);
    // One original guest boundary; false delegates to the original H8.
    bool step(ControllerCpu&);
    static bool candidate(unsigned pc){
        // Cheap page filter in the hot Board loop; step() validates exact PCs.
        constexpr uint64_t pages[]{0x007be03000000000ull,0x0180000000000001ull,0x0000007e001f8000ull,0x0000000000000000ull};
        return pc<0x10000 && ((pages[pc>>14]>>((pc>>8)&63))&1);
    }
    LiveNoteController noteActions;
    LiveVoiceController voiceActions;
    ParameterScaleCalls parameterScaleCalls;
    DspFaultCalls faultCalls;
    DspBootStreamCalls bootStreamCalls;
    DspBootSetupCalls bootSetupCalls;
    DspStartupCalls startupCalls;
    ScheduledTimerCalls scheduledTimerCalls;
    ServiceCycleCalls serviceCycleCalls;
    // Count actual original-core calls, including interrupt arbitration. These
    // are not instruction counts or CPU time, and never imply full migration.
    void delegated(unsigned pc){++originalStepCalls;if(!originalPages.empty())++originalPages[(pc&0xffffff)>>8];}
    void enableDelegationProfile(){originalPages.assign(65536,0);originalStepCalls=0;}
    void resetMetrics(){
        nativeSteps=bytes=messages=noteOn=noteOff=controls=originalStepCalls=0;
        noteActions.resetMetrics();parameterScaleCalls.resetMetrics();faultCalls.resetMetrics();bootStreamCalls.resetMetrics();bootSetupCalls.resetMetrics();startupCalls.resetMetrics();scheduledTimerCalls.resetMetrics();serviceCycleCalls.resetMetrics();voiceActions.resetMetrics();
        if(!originalPages.empty())std::fill(originalPages.begin(),originalPages.end(),0);
    }
    uint64_t originalStepCalls=0;
    // Detailed page accounting is fixture-only: no allocation or page writes
    // in a normal experimental plugin instance. Heap storage avoids the small
    // default Windows stack when a probe explicitly enables it.
    std::vector<uint64_t> originalPages;
    uint64_t nativeSteps=0,bytes=0,messages=0,noteOn=0,noteOff=0,controls=0;
private:
    bool decoding=false;
    static bool supported(uint8_t status);
};
}
