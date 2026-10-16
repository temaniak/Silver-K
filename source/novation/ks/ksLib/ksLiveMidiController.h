#pragma once
#include "ksControllerState.h"
#include "ksMidiDecodeCalls.h"
#include "ksMidiControlCalls.h"
#include "ksMidiAuxCalls.h"
#include "ksHardwareSetupCalls.h"
#include "ksMidiPartInitCalls.h"
#include "ksMidiRouteCalls.h"
#include "ksStateFieldMapCalls.h"
#include "ksVoiceTableInitCalls.h"
#include "ksStateParameterEditCalls.h"
#include "ksStatePatchNameCalls.h"
#include "ksStateStorageCalls.h"
#include "ksPanelDecayCalls.h"
#include "ksStateModeCalls.h"
#include "ksStateParameterExpandCalls.h"
#include "ksStatePackFieldsCalls.h"
#include "ksStatePackRefreshCalls.h"
#include "ksStateGlobalsCalls.h"
#include "ksPanelByteCalls.h"
#include "ksPanelPollCalls.h"
#include "ksPanelActionCalls.h"
#include "ksArpPrimitiveCalls.h"
#include "ksArpScheduleCalls.h"
#include "ksArpMainCalls.h"
#include "ksStateMultiSerializeCalls.h"
#include "ksStatePatchLoadCalls.h"
#include "ksStateProgramCalls.h"
#include "ksStateMultiLoadCalls.h"
#include "ksStatePresetAddressCalls.h"
#include "ksStateSingleLoadCalls.h"
#include "ksSerialQueueCalls.h"
#include "ksSerialReceiveCalls.h"
#include "ksSerialSetupCalls.h"
#include "ksSerialTransmitCalls.h"
#include "ksSerialIrqCalls.h"
#include "ksSerialDirectCalls.h"
#include "ksArpEventCalls.h"
#include "ksArpSelectCalls.h"
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
#include "ksStateInitCalls.h"
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
        return MidiPartInitCalls::candidate(pc)||MidiRouteCalls::candidate(pc)||MidiDecodeCalls::candidate(pc)||MidiControlCalls::candidate(pc)||MidiAuxCalls::candidate(pc)||(pc<0x10000 && ((pages[pc>>14]>>((pc>>8)&63))&1));
    }
    MidiDecodeCalls midiDecodeCalls;
    MidiControlCalls midiControlCalls;
    MidiAuxCalls midiAuxCalls;
    HardwareSetupCalls hardwareSetupCalls;
    MidiPartInitCalls midiPartInitCalls;
    MidiRouteCalls midiRouteCalls;
    StateFieldMapCalls stateFieldMapCalls;
    VoiceTableInitCalls voiceTableInitCalls;
    StateParameterEditCalls stateParameterEditCalls;
    StatePatchNameCalls statePatchNameCalls;
    StateStorageCalls stateStorageCalls;
    PanelDecayCalls panelDecayCalls;
    StateModeCalls stateModeCalls;
    StateParameterExpandCalls stateParameterExpandCalls;
    StatePackFieldsCalls statePackFieldsCalls;
    StatePackRefreshCalls statePackRefreshCalls;
    StateGlobalsCalls stateGlobalsCalls;
    PanelByteCalls panelByteCalls;
    PanelPollCalls panelPollCalls;
    PanelActionCalls panelActionCalls;
    ArpPrimitiveCalls arpPrimitiveCalls;
    ArpScheduleCalls arpScheduleCalls;
    ArpMainCalls arpMainCalls;
    StateMultiSerializeCalls stateMultiSerializeCalls;
    StatePatchLoadCalls statePatchLoadCalls;
    StateProgramCalls stateProgramCalls;
    StateMultiLoadCalls stateMultiLoadCalls;
    StatePresetAddressCalls statePresetAddressCalls;
    StateSingleLoadCalls stateSingleLoadCalls;
    SerialQueueCalls serialQueueCalls;
    SerialReceiveCalls serialReceiveCalls;
    SerialSetupCalls serialSetupCalls;
    SerialTransmitCalls serialTransmitCalls;
    SerialIrqCalls serialIrqCalls;
    SerialDirectCalls serialDirectCalls;
    ArpEventCalls arpEventCalls;
    ArpSelectCalls arpSelectCalls;
    LiveNoteController noteActions;
    LiveVoiceController voiceActions;
    ParameterScaleCalls parameterScaleCalls;
    DspFaultCalls faultCalls;
    DspBootStreamCalls bootStreamCalls;
    DspBootSetupCalls bootSetupCalls;
    DspStartupCalls startupCalls;
    ScheduledTimerCalls scheduledTimerCalls;
    ServiceCycleCalls serviceCycleCalls;
    StateInitCalls stateInitCalls;
    // Count actual original-core calls, including interrupt arbitration. These
    // are not instruction counts or CPU time, and never imply full migration.
    void delegated(unsigned pc){++originalStepCalls;if(!originalPages.empty())++originalPages[(pc&0xffffff)>>8];}
    void enableDelegationProfile(){originalPages.assign(65536,0);originalStepCalls=0;}
    void resetMetrics(){
        midiDecodeCalls.resetMetrics();
        midiControlCalls.resetMetrics();
        midiAuxCalls.resetMetrics();
        hardwareSetupCalls.resetMetrics();
        midiPartInitCalls.resetMetrics();
        midiRouteCalls.resetMetrics();
        stateFieldMapCalls.resetMetrics();
        voiceTableInitCalls.resetMetrics();
        stateParameterEditCalls.resetMetrics();
        statePatchNameCalls.resetMetrics();
        stateStorageCalls.resetMetrics();
        panelDecayCalls.resetMetrics();
        stateModeCalls.resetMetrics();
        stateParameterExpandCalls.resetMetrics();
        statePackFieldsCalls.resetMetrics();
        statePackRefreshCalls.resetMetrics();
        stateGlobalsCalls.resetMetrics();
        panelByteCalls.resetMetrics();
        panelPollCalls.resetMetrics();
        panelActionCalls.resetMetrics();
        arpPrimitiveCalls.resetMetrics();
        arpScheduleCalls.resetMetrics();
        arpMainCalls.resetMetrics();
        stateMultiSerializeCalls.resetMetrics();
        statePatchLoadCalls.resetMetrics();
        stateProgramCalls.resetMetrics();
        stateMultiLoadCalls.resetMetrics();
        statePresetAddressCalls.resetMetrics();
        stateSingleLoadCalls.resetMetrics();
        serialQueueCalls.resetMetrics();
        serialReceiveCalls.resetMetrics();
        serialSetupCalls.resetMetrics();
        serialTransmitCalls.resetMetrics();
        serialIrqCalls.resetMetrics();
        serialDirectCalls.resetMetrics();
        arpEventCalls.resetMetrics();
        arpSelectCalls.resetMetrics();
        nativeSteps=bytes=messages=noteOn=noteOff=controls=originalStepCalls=0;
        noteActions.resetMetrics();parameterScaleCalls.resetMetrics();faultCalls.resetMetrics();bootStreamCalls.resetMetrics();bootSetupCalls.resetMetrics();startupCalls.resetMetrics();scheduledTimerCalls.resetMetrics();serviceCycleCalls.resetMetrics();stateInitCalls.resetMetrics();voiceActions.resetMetrics();
        if(!originalPages.empty())std::fill(originalPages.begin(),originalPages.end(),0);
    }
    uint64_t originalStepCalls=0;
    // Detailed page accounting is fixture-only: no allocation or page writes
    // in a normal experimental plugin instance. Heap storage avoids the small
    // default Windows stack when a probe explicitly enables it.
    std::vector<uint64_t> originalPages;
    uint64_t nativeSteps=0,bytes=0,messages=0,noteOn=0,noteOff=0,controls=0;

};
}
