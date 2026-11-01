#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"

namespace ks {
// Main controller service, preset/part/global edits, serialization and Flash state.
// Fixed native control flow for the verified KS revision. Private ROM data
// remains external; instruction fetch reads account for original bus clocks.
bool RuntimeController::state(ControllerCpu& h){
    if(StateEditAddressCalls::candidate(unsigned(h.getPC())))return shared.stateEditAddressCalls.step(h);
    if(StateStoreUiCalls::candidate(unsigned(h.getPC())))return shared.stateStoreUiCalls.step(h);
    if(StateForegroundCalls::candidate(unsigned(h.getPC())))return shared.stateForegroundCalls.step(h);
    if(StateButtonDispatchCalls::candidate(unsigned(h.getPC())))return shared.stateButtonDispatchCalls.step(h);
    if(StateScreenCalls::candidate(unsigned(h.getPC())))return shared.stateScreenCalls.step(h);
    if(StateEditContextCalls::candidate(unsigned(h.getPC())))return shared.stateEditContextCalls.step(h);
    if(StateScreenEditCalls::candidate(unsigned(h.getPC())))return shared.stateScreenEditCalls.step(h);
    if(StateMenuFormatCalls::candidate(unsigned(h.getPC())))return shared.stateMenuFormatCalls.step(h);
    if(StatePartSelectionCalls::candidate(unsigned(h.getPC())))return shared.statePartSelectionCalls.step(h);
    if(StateMenuActionsCalls::candidate(unsigned(h.getPC())))return shared.stateMenuActionsCalls.step(h);
    if(StateScreenActionsCalls::candidate(unsigned(h.getPC())))return shared.stateScreenActionsCalls.step(h);
    if(StatePanelModesCalls::candidate(unsigned(h.getPC())))return shared.statePanelModesCalls.step(h);
    if(StateFunctionKeysCalls::candidate(unsigned(h.getPC())))return shared.stateFunctionKeysCalls.step(h);
    if(StateEditWorkflowCalls::candidate(unsigned(h.getPC())))return shared.stateEditWorkflowCalls.step(h);
    if(StateValueBoundsCalls::candidate(unsigned(h.getPC())))return shared.stateValueBoundsCalls.step(h);
    if(StateEditLabelCalls::candidate(unsigned(h.getPC())))return shared.stateEditLabelCalls.step(h);
    if(StateFieldActionsCalls::candidate(unsigned(h.getPC())))return shared.stateFieldActionsCalls.step(h);
    if(StateBankNavigationCalls::candidate(unsigned(h.getPC())))return shared.stateBankNavigationCalls.step(h);
    if(StateStatusDisplayCalls::candidate(unsigned(h.getPC())))return shared.stateStatusDisplayCalls.step(h);
    if(StateStoredValueCalls::candidate(unsigned(h.getPC())))return shared.stateStoredValueCalls.step(h);
    if(StateBrowseWorkflowCalls::candidate(unsigned(h.getPC())))return shared.stateBrowseWorkflowCalls.step(h);
    if(StateProgramBrowseCalls::candidate(unsigned(h.getPC())))return shared.stateProgramBrowseCalls.step(h);
    if(StateProgramDisplayCalls::candidate(unsigned(h.getPC())))return shared.stateProgramDisplayCalls.step(h);
    if(StateProgramEventCalls::candidate(unsigned(h.getPC())))return shared.stateProgramEventCalls.step(h);
    if(StateChannelCalls::candidate(unsigned(h.getPC())))return shared.stateChannelCalls.step(h);
    if(StateInitCalls::candidate(unsigned(h.getPC()))&&shared.stateInitCalls.step(h))return true;
    if(ServiceCycleCalls::candidate(unsigned(h.getPC()))&&shared.serviceCycleCalls.step(h))return true;
    if(ScheduledTimerCalls::candidate(unsigned(h.getPC()))&&shared.scheduledTimerCalls.step(h))return true;
    if(StateFieldMapCalls::candidate(unsigned(h.getPC()))&&shared.stateFieldMapCalls.step(h))return true;
    if(StateParameterEditCalls::candidate(unsigned(h.getPC()))&&shared.stateParameterEditCalls.step(h))return true;
    if(StatePatchNameCalls::candidate(unsigned(h.getPC()))&&shared.statePatchNameCalls.step(h))return true;
    if(StateStorageCalls::candidate(unsigned(h.getPC()))&&shared.stateStorageCalls.step(h))return true;
    if(StateModeCalls::candidate(unsigned(h.getPC()))&&shared.stateModeCalls.step(h))return true;
    if(StateParameterExpandCalls::candidate(unsigned(h.getPC()))&&shared.stateParameterExpandCalls.step(h))return true;
    if(StatePackFieldsCalls::candidate(unsigned(h.getPC()))&&shared.statePackFieldsCalls.step(h))return true;
    if(StatePackRefreshCalls::candidate(unsigned(h.getPC()))&&shared.statePackRefreshCalls.step(h))return true;
    if(StateGlobalsCalls::candidate(unsigned(h.getPC()))&&shared.stateGlobalsCalls.step(h))return true;
    if(StateMultiSerializeCalls::candidate(unsigned(h.getPC()))&&shared.stateMultiSerializeCalls.step(h))return true;
    if(StatePatchLoadCalls::candidate(unsigned(h.getPC()))&&shared.statePatchLoadCalls.step(h))return true;
    if(StateProgramCalls::candidate(unsigned(h.getPC()))&&shared.stateProgramCalls.step(h))return true;
    if(StateMultiLoadCalls::candidate(unsigned(h.getPC()))&&shared.stateMultiLoadCalls.step(h))return true;
    if(StatePresetAddressCalls::candidate(unsigned(h.getPC()))&&shared.statePresetAddressCalls.step(h))return true;
    if(StateSingleLoadCalls::candidate(unsigned(h.getPC()))&&shared.stateSingleLoadCalls.step(h))return true;
    if(StateManualEditCalls::candidate(unsigned(h.getPC()))&&shared.stateManualEditCalls.step(h))return true;
    if(StatePostEditCalls::candidate(unsigned(h.getPC()))&&shared.statePostEditCalls.step(h))return true;
    if(StateFlashCommitCalls::candidate(unsigned(h.getPC()))&&shared.stateFlashCommitCalls.step(h))return true;
    if(StateStoreServicesCalls::candidate(unsigned(h.getPC()))&&shared.stateStoreServicesCalls.step(h))return true;
    if(StatePresetFormatCalls::candidate(unsigned(h.getPC()))&&shared.statePresetFormatCalls.step(h))return true;
    if(StatePresetSaveCalls::candidate(unsigned(h.getPC()))&&shared.statePresetSaveCalls.step(h))return true;
    if(StateGlobalSaveCalls::candidate(unsigned(h.getPC()))&&shared.stateGlobalSaveCalls.step(h))return true;
    if(StateFactoryServicesCalls::candidate(unsigned(h.getPC()))&&shared.stateFactoryServicesCalls.step(h))return true;
    if(StateMultiSaveCalls::candidate(unsigned(h.getPC()))&&shared.stateMultiSaveCalls.step(h))return true;
    if(StateNameCodecCalls::candidate(unsigned(h.getPC()))&&shared.stateNameCodecCalls.step(h))return true;
    if(StateDeferredPartsCalls::candidate(unsigned(h.getPC()))&&shared.stateDeferredPartsCalls.step(h))return true;
    if(StatePanelRefreshCalls::candidate(unsigned(h.getPC()))&&shared.statePanelRefreshCalls.step(h))return true;
    if(StateAuditionCalls::candidate(unsigned(h.getPC()))&&shared.stateAuditionCalls.step(h))return true;
    if(StateProgramChangeCalls::candidate(unsigned(h.getPC()))&&shared.stateProgramChangeCalls.step(h))return true;
    if(StateNameEditorCalls::candidate(unsigned(h.getPC()))&&shared.stateNameEditorCalls.step(h))return true;
    if(StateBankDisplayCalls::candidate(unsigned(h.getPC()))&&shared.stateBankDisplayCalls.step(h))return true;
    if(StateDspFactoryTransferCalls::candidate(unsigned(h.getPC()))&&shared.stateDspFactoryTransferCalls.step(h))return true;
    if(StateDspFactoryResetCalls::candidate(unsigned(h.getPC()))&&shared.stateDspFactoryResetCalls.step(h))return true;
    if(StateDspFactoryUploadCalls::candidate(unsigned(h.getPC()))&&shared.stateDspFactoryUploadCalls.step(h))return true;
    return shared.noteActions.step(h);
}
}
