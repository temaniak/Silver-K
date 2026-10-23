cmake_minimum_required(VERSION 3.22)
add_library(ksLib STATIC ksBoard.cpp ksBoard.h ksDSP.cpp ksDSP.h ksDevice.cpp ksDevice.h ksRom.cpp ksRom.h ksPlaceholderMidi.h ksWorkerPlacement.cpp ksWorkerPlacement.h)
target_sources(ksLib PRIVATE ksKeySearchCalls.cpp ksKeySearchCalls.h ksParameterScaleCalls.cpp ksParameterScaleCalls.h)
target_sources(ksLib PRIVATE ksControllerState.cpp ksControllerState.h ksControlArithmetic.h)
target_compile_features(ksLib PUBLIC cxx_std_17)
target_sources(ksLib PRIVATE ksDspWorkers.cpp ksDspWorkers.h)
target_sources(ksLib PRIVATE ksControllerTrace.cpp ksControllerTrace.h)
target_sources(ksLib PRIVATE ksDspTransport.cpp ksControllerJobs.cpp ksDspTransport.h)
target_sources(ksLib PRIVATE ksLiveMidiController.cpp ksLiveMidiController.h)
target_sources(ksLib PRIVATE ksLiveNoteController.cpp ksLiveNoteController.h)
target_sources(ksLib PRIVATE ksLiveVoiceController.cpp ksLiveVoiceController.h ksControllerBoundary.h)
target_include_directories(ksLib PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(ksLib PUBLIC baseLib dsp56kEmu)
# Frequency tracing is a separate diagnostic build; no per-boundary profile
# checks or storage are present in an ordinary Release controller.
option(SILVER_KS_CONTROLLER_PROFILING "Compile KS boundary-frequency diagnostics" OFF)
if(SILVER_KS_CONTROLLER_PROFILING)
 target_compile_definitions(ksLib PUBLIC SILVER_KS_CONTROLLER_PROFILING=1)
endif()

if(BUILD_TESTING)
 add_executable(ksPlaceholderMidi_test ksPlaceholderMidi_test.cpp)
 target_compile_features(ksPlaceholderMidi_test PRIVATE cxx_std_17)
 add_test(NAME ksPlaceholderMidi COMMAND ksPlaceholderMidi_test)
endif()

target_sources(ksLib PRIVATE ksInterruptService.h ksNativeInterrupts.cpp ksRuntimeStartup.cpp ksRuntimeController.cpp ksRuntimeIo.cpp ksRuntimeMidi.cpp ksRuntimeState.cpp ksRuntimeParameters.cpp ksRuntimeVoices.cpp ksRuntimeExtended.cpp ksRuntimeController.h ksRuntimeBoundary.h)

target_sources(ksLib PRIVATE ksDspWordCalls.cpp ksDspWordCalls.h ksDspWordTask.h)

target_sources(ksLib PRIVATE ksDspAddressCalls.cpp ksDspAddressCalls.h ksDspAddressTask.h)

target_sources(ksLib PRIVATE ksDspFaultCalls.cpp ksDspFaultCalls.h ksDspFaultTask.h)

target_sources(ksLib PRIVATE ksDspBootStreamCalls.cpp ksDspBootStreamCalls.h ksDspBootStreamTask.h)

target_sources(ksLib PRIVATE ksDspBootSetupCalls.cpp ksDspBootSetupCalls.h ksDspBootSetupTask.h)

target_sources(ksLib PRIVATE ksDspStartupCalls.cpp)

target_sources(ksLib PRIVATE ksDspRoutingCalls.cpp)

target_sources(ksLib PRIVATE ksPartControlCalls.cpp)

target_sources(ksLib PRIVATE ksVelocityCurveCalls.cpp)

target_sources(ksLib PRIVATE ksVoiceSelectionCalls.cpp)

target_sources(ksLib PRIVATE ksVoicePopulationCalls.cpp)

target_sources(ksLib PRIVATE ksKeyResetCalls.cpp)

target_sources(ksLib PRIVATE ksVoiceReleaseCalls.cpp)

target_sources(ksLib PRIVATE ksUnisonCalls.cpp)

target_sources(ksLib PRIVATE ksVoiceTrackingCalls.cpp)

target_sources(ksLib PRIVATE ksVelocityModulationCalls.cpp)

target_sources(ksLib PRIVATE ksGlideCalls.cpp)

target_sources(ksLib PRIVATE ksVoiceStartCalls.cpp)


target_sources(ksLib PRIVATE ksKeyActivityCalls.cpp)


target_sources(ksLib PRIVATE ksNoteOnStateCalls.cpp)


target_sources(ksLib PRIVATE ksOscillatorModulationCalls.cpp)


target_sources(ksLib PRIVATE ksGlobalModulationCalls.cpp)


target_sources(ksLib PRIVATE ksVoiceSweepCalls.cpp)


target_sources(ksLib PRIVATE ksKeyPriorityCalls.cpp)


target_sources(ksLib PRIVATE ksScheduledTimerCalls.cpp)


target_sources(ksLib PRIVATE ksOscillatorPitchCalls.cpp)


target_sources(ksLib PRIVATE ksParameterPitchCalls.cpp)


target_sources(ksLib PRIVATE ksParameterModulationCalls.cpp)


target_sources(ksLib PRIVATE ksVoiceRefreshCalls.cpp)


target_sources(ksLib PRIVATE ksGlobalSetupCalls.cpp)


target_sources(ksLib PRIVATE ksNoteOffStateCalls.cpp)


target_sources(ksLib PRIVATE ksParameterScalarCalls.cpp)


target_sources(ksLib PRIVATE ksGlobalScalarCalls.cpp)


target_sources(ksLib PRIVATE ksGlobalTimeCalls.cpp)


target_sources(ksLib PRIVATE ksServiceCycleCalls.cpp)


target_sources(ksLib PRIVATE ksParameterTimeCalls.cpp)


target_sources(ksLib PRIVATE ksParameterCurveCalls.cpp)


target_sources(ksLib PRIVATE ksParameterVoiceScaleCalls.cpp)


target_sources(ksLib PRIVATE ksGlobalShapeCalls.cpp)


target_sources(ksLib PRIVATE ksGlobalRangeCalls.cpp)


target_sources(ksLib PRIVATE ksPartNoteCalls.cpp)


target_sources(ksLib PRIVATE ksPedalCalls.cpp)


target_sources(ksLib PRIVATE ksNoteProgramCalls.cpp)


target_sources(ksLib PRIVATE ksStateInitCalls.cpp)

target_sources(ksLib PRIVATE ksParameterHeaderCalls.cpp)

target_sources(ksLib PRIVATE ksParameterSignedSquareCalls.cpp)

target_sources(ksLib PRIVATE ksParameterFilterCalls.cpp)

target_sources(ksLib PRIVATE ksParameterVolumeCalls.cpp)

target_sources(ksLib PRIVATE ksParameterGateCalls.cpp)

target_sources(ksLib PRIVATE ksParameterSupportCalls.cpp)

target_sources(ksLib PRIVATE ksParameterLfoCalls.cpp)

target_sources(ksLib PRIVATE ksGlobalMixCalls.cpp)

target_sources(ksLib PRIVATE ksGlobalTableCalls.cpp)

target_sources(ksLib PRIVATE ksSquareRootCalls.cpp)

target_sources(ksLib PRIVATE ksGlobalMatrixCalls.cpp)

target_sources(ksLib PRIVATE ksGlobalClampedTimeCalls.cpp)

target_sources(ksLib PRIVATE ksGlobalPairCalls.cpp)

target_sources(ksLib PRIVATE ksGlobalTailScalarCalls.cpp)

target_sources(ksLib PRIVATE ksGlobalTimeTailCalls.cpp)

target_sources(ksLib PRIVATE ksGlobalFinishScalarCalls.cpp)

target_sources(ksLib PRIVATE ksGlobalFinalCalls.cpp)

target_sources(ksLib PRIVATE ksMidiDecodeCalls.cpp)

target_sources(ksLib PRIVATE ksMidiControlCalls.cpp)

target_sources(ksLib PRIVATE ksMidiAuxCalls.cpp)

target_sources(ksLib PRIVATE ksHardwareSetupCalls.cpp)

target_sources(ksLib PRIVATE ksMidiPartInitCalls.cpp)

target_sources(ksLib PRIVATE ksMidiRouteCalls.cpp)

target_sources(ksLib PRIVATE ksStateFieldMapCalls.cpp)

target_sources(ksLib PRIVATE ksGlobalEffectCalls.cpp)

target_sources(ksLib PRIVATE ksPartModulationCalls.cpp)

target_sources(ksLib PRIVATE ksPartRouteCalls.cpp)

target_sources(ksLib PRIVATE ksVoiceFeedbackCalls.cpp)

target_sources(ksLib PRIVATE ksVoiceTableInitCalls.cpp)

target_sources(ksLib PRIVATE ksStateParameterEditCalls.cpp)

target_sources(ksLib PRIVATE ksStatePatchNameCalls.cpp)

target_sources(ksLib PRIVATE ksStateStorageCalls.cpp)

target_sources(ksLib PRIVATE ksPartEnableCalls.cpp)

target_sources(ksLib PRIVATE ksPartSignalCalls.cpp)

target_sources(ksLib PRIVATE ksPanelDecayCalls.cpp)

target_sources(ksLib PRIVATE ksStateModeCalls.cpp)

target_sources(ksLib PRIVATE ksStateParameterExpandCalls.cpp)

target_sources(ksLib PRIVATE ksStatePackFieldsCalls.cpp)

target_sources(ksLib PRIVATE ksStatePackRefreshCalls.cpp)

target_sources(ksLib PRIVATE ksStateGlobalsCalls.cpp)

target_sources(ksLib PRIVATE ksPeriodicCalls.cpp)

target_sources(ksLib PRIVATE ksSyncValueCalls.cpp)

target_sources(ksLib PRIVATE ksPartRefreshCalls.cpp)

target_sources(ksLib PRIVATE ksPanelByteCalls.cpp)

target_sources(ksLib PRIVATE ksPanelPollCalls.cpp)

target_sources(ksLib PRIVATE ksPanelActionCalls.cpp)

target_sources(ksLib PRIVATE ksArpPrimitiveCalls.cpp)

target_sources(ksLib PRIVATE ksArpScheduleCalls.cpp)

target_sources(ksLib PRIVATE ksArpMainCalls.cpp)

target_sources(ksLib PRIVATE ksStateMultiSerializeCalls.cpp)

target_sources(ksLib PRIVATE ksStatePatchLoadCalls.cpp)

target_sources(ksLib PRIVATE ksStateProgramCalls.cpp)

target_sources(ksLib PRIVATE ksStateMultiLoadCalls.cpp)

target_sources(ksLib PRIVATE ksStatePresetAddressCalls.cpp)

target_sources(ksLib PRIVATE ksStateSingleLoadCalls.cpp)

target_sources(ksLib PRIVATE ksSerialQueueCalls.cpp)

target_sources(ksLib PRIVATE ksSerialReceiveCalls.cpp)

target_sources(ksLib PRIVATE ksSerialSetupCalls.cpp)

target_sources(ksLib PRIVATE ksSerialTransmitCalls.cpp)

target_sources(ksLib PRIVATE ksSerialIrqCalls.cpp)

target_sources(ksLib PRIVATE ksSerialDirectCalls.cpp)

target_sources(ksLib PRIVATE ksArpEventCalls.cpp)

target_sources(ksLib PRIVATE ksArpSelectCalls.cpp)

target_sources(ksLib PRIVATE ksResetImageCalls.cpp)

target_sources(ksLib PRIVATE ksStateManualEditCalls.cpp)

target_sources(ksLib PRIVATE ksStatePostEditCalls.cpp)

target_sources(ksLib PRIVATE ksPanelControlCalls.cpp)

target_sources(ksLib PRIVATE ksPanelToggleCalls.cpp)

target_sources(ksLib PRIVATE ksPanelDecimalCalls.cpp)

target_sources(ksLib PRIVATE ksPanelTextCalls.cpp)

target_sources(ksLib PRIVATE ksInternalMidiCalls.cpp)

target_sources(ksLib PRIVATE ksInternalNoteCalls.cpp)

target_sources(ksLib PRIVATE ksSysExFrameCalls.cpp)

target_sources(ksLib PRIVATE ksExceptionReturnCalls.cpp)

target_sources(ksLib PRIVATE ksStateFlashCommitCalls.cpp)

target_sources(ksLib PRIVATE ksStateStoreServicesCalls.cpp)

target_sources(ksLib PRIVATE ksStatePresetFormatCalls.cpp)

target_sources(ksLib PRIVATE ksStatePresetSaveCalls.cpp)

target_sources(ksLib PRIVATE ksStateGlobalSaveCalls.cpp)

target_sources(ksLib PRIVATE ksStateFactoryServicesCalls.cpp)

target_sources(ksLib PRIVATE ksPanelFormatCalls.cpp)

target_sources(ksLib PRIVATE ksPanelHexCalls.cpp)

target_sources(ksLib PRIVATE ksPanelPercentCalls.cpp)

target_sources(ksLib PRIVATE ksPanelVersionCalls.cpp)

target_sources(ksLib PRIVATE ksPatchExpandCalls.cpp)

target_sources(ksLib PRIVATE ksSysExActionCalls.cpp)

target_sources(ksLib PRIVATE ksLegacySysExCalls.cpp)

target_sources(ksLib PRIVATE ksStateMultiSaveCalls.cpp)

target_sources(ksLib PRIVATE ksStateNameCodecCalls.cpp)

target_sources(ksLib PRIVATE ksStateDeferredPartsCalls.cpp)

target_sources(ksLib PRIVATE ksStatePanelRefreshCalls.cpp)

target_sources(ksLib PRIVATE ksStateAuditionCalls.cpp)

target_sources(ksLib PRIVATE ksPanelPublishCalls.cpp)

target_sources(ksLib PRIVATE ksPanelMasksCalls.cpp)

target_sources(ksLib PRIVATE ksPanelInitializeCalls.cpp)

target_sources(ksLib PRIVATE ksPanelDetectCalls.cpp)

target_sources(ksLib PRIVATE ksFirmwareLayoutCheckCalls.cpp)

target_sources(ksLib PRIVATE ksSysExDumpCalls.cpp)

target_sources(ksLib PRIVATE ksMidiFileTimeCalls.cpp)

target_sources(ksLib PRIVATE ksStateProgramChangeCalls.cpp)

target_sources(ksLib PRIVATE ksStateNameEditorCalls.cpp)

target_sources(ksLib PRIVATE ksStateBankDisplayCalls.cpp)

target_sources(ksLib PRIVATE ksStateDspFactoryTransferCalls.cpp)

target_sources(ksLib PRIVATE ksStateDspFactoryResetCalls.cpp)

target_sources(ksLib PRIVATE ksStateDspFactoryUploadCalls.cpp)

target_sources(ksLib PRIVATE ksTimingTablesCalls.cpp)

target_sources(ksLib PRIVATE ksMidiFileEventCalls.cpp)

target_sources(ksLib PRIVATE ksMonoArpCalls.cpp)

target_sources(ksLib PRIVATE ksMidiFileTransportCalls.cpp)

target_sources(ksLib PRIVATE ksEditAddressCalls.cpp)

target_sources(ksLib PRIVATE ksEditSceneCalls.cpp)

target_sources(ksLib PRIVATE ksEditDecisionCalls.cpp)

# Native Flash page comparison/programming; original bodies are test-only.
target_sources(ksLib PRIVATE ksFlashPageCalls.cpp ksFlashPageCalls.h ksFlashPageTask.h)

target_sources(ksLib PRIVATE ksTimerServiceCalls.cpp ksTimerServiceCalls.h ksTimerServiceTask.h)

target_sources(ksLib PRIVATE ksStateChannelCalls.cpp ksStateChannelCalls.h ksStateChannelTask.h)

target_sources(ksLib PRIVATE ksPanelInputCalls.cpp ksPanelInputCalls.h ksPanelInputTask.h)

target_sources(ksLib PRIVATE ksStateEditAddressCalls.cpp ksStateEditAddressCalls.h ksStateEditAddressTask.h)

target_sources(ksLib PRIVATE ksStateStoreUiCalls.cpp ksStateStoreUiCalls.h ksStateStoreUiTask.h)

target_sources(ksLib PRIVATE ksPanelMatrixCalls.cpp ksPanelMatrixCalls.h ksPanelMatrixTask.h)

target_sources(ksLib PRIVATE ksPedalSenseCalls.cpp ksPedalSenseCalls.h ksPedalSenseTask.h)

target_sources(ksLib PRIVATE ksPanelEncoderCalls.cpp ksPanelEncoderCalls.h ksPanelEncoderTask.h)

target_sources(ksLib PRIVATE ksPanelAdcSamplesCalls.cpp ksPanelAdcSamplesCalls.h ksPanelAdcSamplesTask.h)

target_sources(ksLib PRIVATE ksPanelAdcScaleCalls.cpp ksPanelAdcScaleCalls.h ksPanelAdcScaleTask.h)

target_sources(ksLib PRIVATE ksPanelAdcCalibrationCalls.cpp ksPanelAdcCalibrationCalls.h ksPanelAdcCalibrationTask.h)

target_sources(ksLib PRIVATE ksPanelAdcIrqCalls.cpp ksPanelAdcIrqCalls.h ksPanelAdcIrqTask.h)

target_sources(ksLib PRIVATE ksPanelAdcScanCalls.cpp ksPanelAdcScanCalls.h ksPanelAdcScanTask.h)

target_sources(ksLib PRIVATE ksInternalMidiQueueCalls.cpp ksInternalMidiQueueCalls.h ksInternalMidiQueueTask.h)

target_sources(ksLib PRIVATE ksMidiOutputCalls.cpp ksMidiOutputCalls.h ksMidiOutputTask.h)

target_sources(ksLib PRIVATE ksInternalMidiServiceCalls.cpp ksInternalMidiServiceCalls.h ksInternalMidiServiceTask.h)

target_sources(ksLib PRIVATE ksStateForegroundCalls.cpp ksStateForegroundCalls.h ksStateForegroundTask.h)

target_sources(ksLib PRIVATE ksStateButtonDispatchCalls.cpp ksStateButtonDispatchCalls.h ksStateButtonDispatchTask.h)

target_sources(ksLib PRIVATE ksPanelAnalogCalls.cpp ksPanelAnalogCalls.h ksPanelAnalogTask.h)

target_sources(ksLib PRIVATE ksClockSyncCalls.cpp ksClockSyncCalls.h ksClockSyncTask.h)

target_sources(ksLib PRIVATE ksClockPulseCalls.cpp ksClockPulseCalls.h ksClockPulseTask.h)

target_sources(ksLib PRIVATE ksClockDisplayCalls.cpp ksClockDisplayCalls.h ksClockDisplayTask.h)

target_sources(ksLib PRIVATE ksStateScreenCalls.cpp ksStateScreenCalls.h ksStateScreenTask.h)

target_sources(ksLib PRIVATE ksPanelSettingsCalls.cpp ksPanelSettingsCalls.h ksPanelSettingsTask.h)

target_sources(ksLib PRIVATE ksStateEditContextCalls.cpp ksStateEditContextCalls.h ksStateEditContextTask.h)

target_sources(ksLib PRIVATE ksStateScreenEditCalls.cpp ksStateScreenEditCalls.h ksStateScreenEditTask.h)

target_sources(ksLib PRIVATE ksStateMenuFormatCalls.cpp ksStateMenuFormatCalls.h ksStateMenuFormatTask.h)

target_sources(ksLib PRIVATE ksSystemServicesCalls.cpp ksSystemServicesCalls.h ksSystemServicesTask.h)

target_sources(ksLib PRIVATE ksKeyboardEventsCalls.cpp ksKeyboardEventsCalls.h ksKeyboardEventsTask.h)

target_sources(ksLib PRIVATE ksStatePartSelectionCalls.cpp ksStatePartSelectionCalls.h ksStatePartSelectionTask.h)

target_sources(ksLib PRIVATE ksStateMenuActionsCalls.cpp ksStateMenuActionsCalls.h ksStateMenuActionsTask.h)

target_sources(ksLib PRIVATE ksKeyboardScanCalls.cpp ksKeyboardScanCalls.h ksKeyboardScanTask.h)

target_sources(ksLib PRIVATE ksDiagnosticCodecCalls.cpp ksDiagnosticCodecCalls.h ksDiagnosticCodecTask.h)

target_sources(ksLib PRIVATE ksDiagnosticTransferCalls.cpp ksDiagnosticTransferCalls.h ksDiagnosticTransferTask.h)

target_sources(ksLib PRIVATE ksMemoryDiagnosticsCalls.cpp ksMemoryDiagnosticsCalls.h ksMemoryDiagnosticsTask.h)

target_sources(ksLib PRIVATE ksStartupBannerCalls.cpp ksStartupBannerCalls.h ksStartupBannerTask.h)

target_sources(ksLib PRIVATE ksStateScreenActionsCalls.cpp ksStateScreenActionsCalls.h ksStateScreenActionsTask.h)

target_sources(ksLib PRIVATE ksStatePanelModesCalls.cpp ksStatePanelModesCalls.h ksStatePanelModesTask.h)

target_sources(ksLib PRIVATE ksStateFunctionKeysCalls.cpp ksStateFunctionKeysCalls.h ksStateFunctionKeysTask.h)

target_sources(ksLib PRIVATE ksStateEditWorkflowCalls.cpp ksStateEditWorkflowCalls.h ksStateEditWorkflowTask.h)

target_sources(ksLib PRIVATE ksLoaderInputsCalls.cpp ksLoaderInputsCalls.h ksLoaderInputsTask.h)

target_sources(ksLib PRIVATE ksLoaderCodecCalls.cpp ksLoaderCodecCalls.h ksLoaderCodecTask.h)

target_sources(ksLib PRIVATE ksLoaderPacketsCalls.cpp ksLoaderPacketsCalls.h ksLoaderPacketsTask.h)

target_sources(ksLib PRIVATE ksLoaderProgramCalls.cpp ksLoaderProgramCalls.h ksLoaderProgramTask.h)

target_sources(ksLib PRIVATE ksLoaderSessionCalls.cpp ksLoaderSessionCalls.h ksLoaderSessionTask.h)

target_sources(ksLib PRIVATE ksStateValueBoundsCalls.cpp ksStateValueBoundsCalls.h ksStateValueBoundsTask.h)

target_sources(ksLib PRIVATE ksStateEditLabelCalls.cpp ksStateEditLabelCalls.h ksStateEditLabelTask.h)

target_sources(ksLib PRIVATE ksStateFieldActionsCalls.cpp ksStateFieldActionsCalls.h ksStateFieldActionsTask.h)

target_sources(ksLib PRIVATE ksStateBankNavigationCalls.cpp ksStateBankNavigationCalls.h ksStateBankNavigationTask.h)

target_sources(ksLib PRIVATE ksStateStatusDisplayCalls.cpp ksStateStatusDisplayCalls.h ksStateStatusDisplayTask.h)

target_sources(ksLib PRIVATE ksStateStoredValueCalls.cpp ksStateStoredValueCalls.h ksStateStoredValueTask.h)

target_sources(ksLib PRIVATE ksStateBrowseWorkflowCalls.cpp ksStateBrowseWorkflowCalls.h ksStateBrowseWorkflowTask.h)

target_sources(ksLib PRIVATE ksStateProgramBrowseCalls.cpp ksStateProgramBrowseCalls.h ksStateProgramBrowseTask.h)

target_sources(ksLib PRIVATE ksStateProgramDisplayCalls.cpp ksStateProgramDisplayCalls.h ksStateProgramDisplayTask.h)

target_sources(ksLib PRIVATE ksStateProgramEventCalls.cpp ksStateProgramEventCalls.h ksStateProgramEventTask.h)
