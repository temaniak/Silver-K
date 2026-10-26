# Private source partition proposal; configure-only validation is separate.
# Include once, then call ks_schedule_controller_source_partition(actual_ks_dir).
# No production switch is authorized until actual cold-path GO.
include_guard(GLOBAL)
set(KS_NATIVE_CONTROLLER_CPP
  "ksBoard.cpp"
  "ksDSP.cpp"
  "ksDevice.cpp"
  "ksRom.cpp"
  "ksWorkerPlacement.cpp"
  "ksDspWorkers.cpp"
  "ksControllerTrace.cpp"
  "ksBoardComparisonBoundary.cpp"
  "ksBoardNativeOwner.cpp"
  "ksBoardResourceCheckpoint.cpp"
)
set(KS_COMPATIBILITY_CONTROLLER_CPP
  "ksArpEventCalls.cpp"
  "ksArpMainCalls.cpp"
  "ksArpPrimitiveCalls.cpp"
  "ksArpScheduleCalls.cpp"
  "ksArpSelectCalls.cpp"
  "ksClockDisplayCalls.cpp"
  "ksClockPulseCalls.cpp"
  "ksClockSyncCalls.cpp"
  "ksClockSyncForeground.cpp"
  "ksControllerJobs.cpp"
  "ksControllerState.cpp"
  "ksDiagnosticCodecCalls.cpp"
  "ksDiagnosticCodecForeground.cpp"
  "ksDiagnosticTransferCalls.cpp"
  "ksDiagnosticTransferForeground.cpp"
  "ksDspAddressCalls.cpp"
  "ksDspBootSetupCalls.cpp"
  "ksDspBootStreamCalls.cpp"
  "ksDspFaultCalls.cpp"
  "ksDspRoutingCalls.cpp"
  "ksDspStartupCalls.cpp"
  "ksDspTransport.cpp"
  "ksDspWordCalls.cpp"
  "ksEditAddressCalls.cpp"
  "ksEditDecisionCalls.cpp"
  "ksEditSceneCalls.cpp"
  "ksExceptionReturnCalls.cpp"
  "ksFirmwareLayoutCheckCalls.cpp"
  "ksFlashPageCalls.cpp"
  "ksGlideCalls.cpp"
  "ksGlobalClampedTimeCalls.cpp"
  "ksGlobalEffectCalls.cpp"
  "ksGlobalFinalCalls.cpp"
  "ksGlobalFinishScalarCalls.cpp"
  "ksGlobalMatrixCalls.cpp"
  "ksGlobalMixCalls.cpp"
  "ksGlobalModulationCalls.cpp"
  "ksGlobalPairCalls.cpp"
  "ksGlobalRangeCalls.cpp"
  "ksGlobalScalarCalls.cpp"
  "ksGlobalSetupCalls.cpp"
  "ksGlobalShapeCalls.cpp"
  "ksGlobalTableCalls.cpp"
  "ksGlobalTailScalarCalls.cpp"
  "ksGlobalTimeCalls.cpp"
  "ksGlobalTimeTailCalls.cpp"
  "ksGlobalUpdateForeground.cpp"
  "ksHardwareSetupCalls.cpp"
  "ksInternalKeyOutputForeground.cpp"
  "ksInternalMidiCalls.cpp"
  "ksInternalMidiQueueCalls.cpp"
  "ksInternalMidiServiceCalls.cpp"
  "ksInternalNoteCalls.cpp"
  "ksKeyActivityCalls.cpp"
  "ksKeyPriorityCalls.cpp"
  "ksKeyResetCalls.cpp"
  "ksKeySearchCalls.cpp"
  "ksKeyboardEventsCalls.cpp"
  "ksKeyboardScanCalls.cpp"
  "ksLegacySysExCalls.cpp"
  "ksLiveMidiController.cpp"
  "ksLiveNoteController.cpp"
  "ksLiveVoiceController.cpp"
  "ksLoaderCodecCalls.cpp"
  "ksLoaderInputsCalls.cpp"
  "ksLoaderPacketsCalls.cpp"
  "ksLoaderProgramCalls.cpp"
  "ksLoaderSessionCalls.cpp"
  "ksMemoryDiagnosticsCalls.cpp"
  "ksMidiAuxCalls.cpp"
  "ksMidiControlCalls.cpp"
  "ksMidiDecodeCalls.cpp"
  "ksMidiFileEventCalls.cpp"
  "ksMidiFileEventForeground.cpp"
  "ksMidiFileTimeCalls.cpp"
  "ksMidiFileTimeForeground.cpp"
  "ksMidiFileTransportCalls.cpp"
  "ksMidiOutputCalls.cpp"
  "ksMidiPacketForeground.cpp"
  "ksMidiPartInitCalls.cpp"
  "ksMidiRouteCalls.cpp"
  "ksMonoArpCalls.cpp"
  "ksNativeArpChordForeground.cpp"
  "ksNativeArpEmissionForeground.cpp"
  "ksNativeArpEmitForeground.cpp"
  "ksNativeArpGateForeground.cpp"
  "ksNativeArpOrderedForeground.cpp"
  "ksNativeArpPrimitiveForeground.cpp"
  "ksNativeArpRandomForeground.cpp"
  "ksNativeArpResetForeground.cpp"
  "ksNativeArpRunForeground.cpp"
  "ksNativeArpSearchForeground.cpp"
  "ksNativeInternalNoteForeground.cpp"
  "ksNativeInterrupts.cpp"
  "ksNativeKeyForeground.cpp"
  "ksNativeMidiAuxForeground.cpp"
  "ksNativeMidiControlForeground.cpp"
  "ksNativeMidiPipelineForeground.cpp"
  "ksNativeMonoArpBootstrapForeground.cpp"
  "ksNativeNoteProgramForeground.cpp"
  "ksNativeNoteStateForeground.cpp"
  "ksNativeParameterEditForeground.cpp"
  "ksNativeParameterSupportForeground.cpp"
  "ksNativePartNoteForeground.cpp"
  "ksNativePedalControlForeground.cpp"
  "ksNativeProgramEventForeground.cpp"
  "ksNativeVoiceAllocationForeground.cpp"
  "ksNativeVoiceModulationForeground.cpp"
  "ksNativeVoiceReleaseForeground.cpp"
  "ksNativeVoiceStartForeground.cpp"
  "ksNativeVoiceSweepForeground.cpp"
  "ksNoteOffStateCalls.cpp"
  "ksNoteOnStateCalls.cpp"
  "ksNoteProgramCalls.cpp"
  "ksOscillatorModulationCalls.cpp"
  "ksOscillatorPitchCalls.cpp"
  "ksPanelActionCalls.cpp"
  "ksPanelAdcCalibrationCalls.cpp"
  "ksPanelAdcIrqCalls.cpp"
  "ksPanelAdcSamplesCalls.cpp"
  "ksPanelAdcScaleCalls.cpp"
  "ksPanelAdcScanCalls.cpp"
  "ksPanelAnalogCalls.cpp"
  "ksPanelByteCalls.cpp"
  "ksPanelControlCalls.cpp"
  "ksPanelControlsForeground.cpp"
  "ksPanelDecayCalls.cpp"
  "ksPanelDecimalCalls.cpp"
  "ksPanelDetectCalls.cpp"
  "ksPanelEncoderCalls.cpp"
  "ksPanelFormatCalls.cpp"
  "ksPanelHexCalls.cpp"
  "ksPanelInitializeCalls.cpp"
  "ksPanelInputCalls.cpp"
  "ksPanelMasksCalls.cpp"
  "ksPanelMasksForeground.cpp"
  "ksPanelMatrixCalls.cpp"
  "ksPanelMonitorForeground.cpp"
  "ksPanelPedalOutputForeground.cpp"
  "ksPanelPercentCalls.cpp"
  "ksPanelPollCalls.cpp"
  "ksPanelPublishCalls.cpp"
  "ksPanelScanForeground.cpp"
  "ksPanelSettingsCalls.cpp"
  "ksPanelSettingsForeground.cpp"
  "ksPanelTextCalls.cpp"
  "ksPanelToggleCalls.cpp"
  "ksPanelVersionCalls.cpp"
  "ksParameterCurveCalls.cpp"
  "ksParameterFilterCalls.cpp"
  "ksParameterGateCalls.cpp"
  "ksParameterHeaderCalls.cpp"
  "ksParameterLfoCalls.cpp"
  "ksParameterModulationCalls.cpp"
  "ksParameterPitchCalls.cpp"
  "ksParameterScalarCalls.cpp"
  "ksParameterScaleCalls.cpp"
  "ksParameterSignedSquareCalls.cpp"
  "ksParameterSupportCalls.cpp"
  "ksParameterTimeCalls.cpp"
  "ksParameterUploadForeground.cpp"
  "ksParameterVoiceScaleCalls.cpp"
  "ksParameterVolumeCalls.cpp"
  "ksPartControlCalls.cpp"
  "ksPartEnableCalls.cpp"
  "ksPartFeedbackForeground.cpp"
  "ksPartModulationCalls.cpp"
  "ksPartNoteCalls.cpp"
  "ksPartRefreshCalls.cpp"
  "ksPartRouteCalls.cpp"
  "ksPartSignalCalls.cpp"
  "ksPatchExpandCalls.cpp"
  "ksPedalCalls.cpp"
  "ksPedalSenseCalls.cpp"
  "ksPeriodicCalls.cpp"
  "ksPeriodicForeground.cpp"
  "ksResetImageCalls.cpp"
  "ksRuntimeController.cpp"
  "ksRuntimeExtended.cpp"
  "ksRuntimeIo.cpp"
  "ksRuntimeMidi.cpp"
  "ksRuntimeParameters.cpp"
  "ksRuntimeStartup.cpp"
  "ksRuntimeState.cpp"
  "ksRuntimeVoices.cpp"
  "ksScheduledTimerCalls.cpp"
  "ksScheduledTimerForeground.cpp"
  "ksSerialDirectCalls.cpp"
  "ksSerialForeground.cpp"
  "ksSerialIrqCalls.cpp"
  "ksSerialQueueCalls.cpp"
  "ksSerialReceiveCalls.cpp"
  "ksSerialSetupCalls.cpp"
  "ksSerialTransmitCalls.cpp"
  "ksServiceCycleCalls.cpp"
  "ksServiceCycleForeground.cpp"
  "ksSquareRootCalls.cpp"
  "ksStartupBannerCalls.cpp"
  "ksStateApplyForeground.cpp"
  "ksStateAuditionCalls.cpp"
  "ksStateBankDisplayCalls.cpp"
  "ksStateBankNavigationCalls.cpp"
  "ksStateBrowseWorkflowCalls.cpp"
  "ksStateButtonDispatchCalls.cpp"
  "ksStateChannelCalls.cpp"
  "ksStateChannelForeground.cpp"
  "ksStateDeferredPartsCalls.cpp"
  "ksStateDeferredPartsForeground.cpp"
  "ksStateDisplayForeground.cpp"
  "ksStateDspFactoryResetCalls.cpp"
  "ksStateDspFactoryTransferCalls.cpp"
  "ksStateDspFactoryUploadCalls.cpp"
  "ksStateEditActionForeground.cpp"
  "ksStateEditAddressCalls.cpp"
  "ksStateEditContextCalls.cpp"
  "ksStateEditLabelCalls.cpp"
  "ksStateEditWorkflowCalls.cpp"
  "ksStateEncoderForeground.cpp"
  "ksStateEventsForeground.cpp"
  "ksStateFactoryForeground.cpp"
  "ksStateFactoryServicesCalls.cpp"
  "ksStateFieldActionsCalls.cpp"
  "ksStateFieldForeground.cpp"
  "ksStateFieldMapCalls.cpp"
  "ksStateFlashCommitCalls.cpp"
  "ksStateForegroundCalls.cpp"
  "ksStateFunctionKeysCalls.cpp"
  "ksStateGlobalSaveCalls.cpp"
  "ksStateGlobalsCalls.cpp"
  "ksStateInitCalls.cpp"
  "ksStateManualEditCalls.cpp"
  "ksStateMenuActionsCalls.cpp"
  "ksStateMenuForeground.cpp"
  "ksStateMenuFormatCalls.cpp"
  "ksStateModeCalls.cpp"
  "ksStateMultiLoadCalls.cpp"
  "ksStateMultiSaveCalls.cpp"
  "ksStateMultiSerializeCalls.cpp"
  "ksStateNameCodecCalls.cpp"
  "ksStateNameEditorCalls.cpp"
  "ksStatePackFieldsCalls.cpp"
  "ksStatePackRefreshCalls.cpp"
  "ksStatePanelForeground.cpp"
  "ksStatePanelModesCalls.cpp"
  "ksStatePanelRefreshCalls.cpp"
  "ksStateParameterEditCalls.cpp"
  "ksStateParameterExpandCalls.cpp"
  "ksStatePartNavigationForeground.cpp"
  "ksStatePartSelectionCalls.cpp"
  "ksStatePatchLoadCalls.cpp"
  "ksStatePatchNameCalls.cpp"
  "ksStatePlaybackModeForeground.cpp"
  "ksStatePostEditCalls.cpp"
  "ksStatePresetAddressCalls.cpp"
  "ksStatePresetFormatCalls.cpp"
  "ksStatePresetSaveCalls.cpp"
  "ksStateProgramBrowseCalls.cpp"
  "ksStateProgramBrowserForeground.cpp"
  "ksStateProgramCalls.cpp"
  "ksStateProgramChangeCalls.cpp"
  "ksStateProgramDisplayCalls.cpp"
  "ksStateProgramEventCalls.cpp"
  "ksStateProgramLoadForeground.cpp"
  "ksStateRefreshForeground.cpp"
  "ksStateResetForeground.cpp"
  "ksStateSaveForeground.cpp"
  "ksStateSceneEditForeground.cpp"
  "ksStateSceneForeground.cpp"
  "ksStateSceneToggleForeground.cpp"
  "ksStateScreenActionsCalls.cpp"
  "ksStateScreenCalls.cpp"
  "ksStateScreenEditCalls.cpp"
  "ksStateScreenForeground.cpp"
  "ksStateSingleLoadCalls.cpp"
  "ksStateStatusDisplayCalls.cpp"
  "ksStateStatusForeground.cpp"
  "ksStateStorageCalls.cpp"
  "ksStateStoreForeground.cpp"
  "ksStateStoreMenuForeground.cpp"
  "ksStateStoreServicesCalls.cpp"
  "ksStateStoreUiCalls.cpp"
  "ksStateStoredValueCalls.cpp"
  "ksStateTickForeground.cpp"
  "ksStateValueBoundsCalls.cpp"
  "ksStateVoiceRefreshForeground.cpp"
  "ksSyncValueCalls.cpp"
  "ksSysExActionCalls.cpp"
  "ksSysExDumpCalls.cpp"
  "ksSysExFrameCalls.cpp"
  "ksSystemServicesCalls.cpp"
  "ksTimerServiceCalls.cpp"
  "ksTimingTablesCalls.cpp"
  "ksUnisonCalls.cpp"
  "ksVelocityCurveCalls.cpp"
  "ksVelocityModulationCalls.cpp"
  "ksVoiceFeedbackCalls.cpp"
  "ksVoiceFeedbackForeground.cpp"
  "ksVoicePopulationCalls.cpp"
  "ksVoiceRefreshCalls.cpp"
  "ksVoiceReleaseCalls.cpp"
  "ksVoiceSelectionCalls.cpp"
  "ksVoiceStartCalls.cpp"
  "ksVoiceSweepCalls.cpp"
  "ksVoiceTableInitCalls.cpp"
  "ksVoiceTrackingCalls.cpp"
  "ksRecordedController_test.cpp"
)
set(KS_LEGACY_COMPATIBILITY_TEST_TARGETS
  "ksLegacyCompatibilityGuard_test"
  "ksAdcService_test"
  "ksArpEventCalls_test"
  "ksArpMainCalls_test"
  "ksArpPrimitiveCalls_test"
  "ksArpScheduleCalls_test"
  "ksArpSelectCalls_test"
  "ksBoardSilence_test"
  "ksClockDisplayCalls_test"
  "ksClockPulseCalls_test"
  "ksClockSyncCalls_test"
  "ksClockSyncService_test"
  "ksControllerBoot_test"
  "ksControllerMemory_test"
  "ksControllerProbe_test"
  "ksControllerTiming_test"
  "ksControllerUart_test"
  "ksCpuCost_test"
  "ksDiagnosticCodecCalls_test"
  "ksDiagnosticCodecService_test"
  "ksDiagnosticTransferCalls_test"
  "ksDiagnosticTransferService_test"
  "ksDspAddressCalls_test"
  "ksDspBootSetupCalls_test"
  "ksDspBootStreamCalls_test"
  "ksDspEquivalence_test"
  "ksDspFaultCalls_test"
  "ksDspRoutingCalls_test"
  "ksDspStartupCalls_test"
  "ksDspWordCalls_test"
  "ksEditAddressCalls_test"
  "ksEditDecisionCalls_test"
  "ksEditSceneCalls_test"
  "ksExceptionReturnCalls_test"
  "ksFirmwareLayoutCheckCalls_test"
  "ksFlashPageCalls_test"
  "ksGlideCalls_test"
  "ksGlobalClampedTimeCalls_test"
  "ksGlobalEffectCalls_test"
  "ksGlobalFinalCalls_test"
  "ksGlobalFinishScalarCalls_test"
  "ksGlobalMatrixCalls_test"
  "ksGlobalMixCalls_test"
  "ksGlobalModulationCalls_test"
  "ksGlobalPairCalls_test"
  "ksGlobalRangeCalls_test"
  "ksGlobalScalarCalls_test"
  "ksGlobalSetupCalls_test"
  "ksGlobalShapeCalls_test"
  "ksGlobalTableCalls_test"
  "ksGlobalTailScalarCalls_test"
  "ksGlobalTimeCalls_test"
  "ksGlobalTimeTailCalls_test"
  "ksGlobalUpdateService_test"
  "ksH8Reference"
  "ksHardwareSetupCalls_test"
  "ksInternalKeyOutputService_test"
  "ksInternalMidiCalls_test"
  "ksInternalMidiQueueCalls_test"
  "ksInternalMidiServiceCalls_test"
  "ksInternalNoteCalls_test"
  "ksInterruptHandoff_test"
  "ksInterruptTimeline_test"
  "ksKeyActivityCalls_test"
  "ksKeyPriorityCalls_test"
  "ksKeyResetCalls_test"
  "ksKeySearchCalls_test"
  "ksKeyboardEventsCalls_test"
  "ksKeyboardScanCalls_test"
  "ksLegacySysExCalls_test"
  "ksLiveMidiController_test"
  "ksLoaderCodecCalls_test"
  "ksLoaderInputsCalls_test"
  "ksLoaderPacketsCalls_test"
  "ksLoaderProgramCalls_test"
  "ksLoaderSessionCalls_test"
  "ksMemoryDiagnosticsCalls_test"
  "ksMidiAuxCalls_test"
  "ksMidiControlCalls_test"
  "ksMidiDecodeCalls_test"
  "ksMidiFileEventCalls_test"
  "ksMidiFileEventServiceTask_test"
  "ksMidiFileEventService_test"
  "ksMidiFileTimeCalls_test"
  "ksMidiFileTimeService_test"
  "ksMidiFileTransportCalls_test"
  "ksMidiFileTransportServiceTask_test"
  "ksMidiFileTransportService_test"
  "ksMidiOutputCalls_test"
  "ksMidiPacketService_test"
  "ksMidiPartInitCalls_test"
  "ksMidiRouteCalls_test"
  "ksMonoArpCalls_test"
  "ksNativeArpChordServiceTask_test"
  "ksNativeArpChordService_test"
  "ksNativeArpEmissionServiceTask_test"
  "ksNativeArpEmissionService_test"
  "ksNativeArpEmitServiceTask_test"
  "ksNativeArpEmitService_test"
  "ksNativeArpGateService_test"
  "ksNativeArpOrderedServiceTask_test"
  "ksNativeArpOrderedService_test"
  "ksNativeArpOwnedForeground_test"
  "ksNativeArpOwnedServiceTask_test"
  "ksNativeArpPrimitiveService_test"
  "ksNativeArpRandomService_test"
  "ksNativeArpResetService_test"
  "ksNativeArpRunServiceTask_test"
  "ksNativeArpRunService_test"
  "ksNativeArpSearchServiceTask_test"
  "ksNativeArpSearchService_test"
  "ksNativeInternalNoteService_test"
  "ksNativeInterrupts_test"
  "ksNativeKeyService_test"
  "ksNativeMidiAuxService_test"
  "ksNativeMidiControlService_test"
  "ksNativeMidiPipelineServiceTask_test"
  "ksNativeMidiPipelineService_test"
  "ksNativeMonoArpBootstrapService_test"
  "ksNativeMonoArpCodeGuard_test"
  "ksNativeMonoArpFactory_test"
  "ksNativeMonoArpOwnedService_test"
  "ksNativeNoteProgramServiceTask_test"
  "ksNativeNoteProgramService_test"
  "ksNativeNoteStateService_test"
  "ksNativeParameterEditService_test"
  "ksNativeParameterSupportService_test"
  "ksNativePartNoteService_test"
  "ksNativePedalControlService_test"
  "ksNativeProgramEventService_test"
  "ksNativeVoiceAllocationService_test"
  "ksNativeVoiceModulationService_test"
  "ksNativeVoiceReleaseService_test"
  "ksNativeVoiceStartServiceTask_test"
  "ksNativeVoiceStartService_test"
  "ksNativeVoiceSweepService_test"
  "ksNoteOffStateCalls_test"
  "ksNoteOnStateCalls_test"
  "ksNoteProgramCalls_test"
  "ksOscillatorModulationCalls_test"
  "ksOscillatorPitchCalls_test"
  "ksPanelActionCalls_test"
  "ksPanelAdcCalibrationCalls_test"
  "ksPanelAdcIrqCalls_test"
  "ksPanelAdcSamplesCalls_test"
  "ksPanelAdcScaleCalls_test"
  "ksPanelAdcScanCalls_test"
  "ksPanelAnalogCalls_test"
  "ksPanelByteCalls_test"
  "ksPanelControlCalls_test"
  "ksPanelControlsService_test"
  "ksPanelDecayCalls_test"
  "ksPanelDecimalCalls_test"
  "ksPanelDetectCalls_test"
  "ksPanelEncoderCalls_test"
  "ksPanelFormatCalls_test"
  "ksPanelHexCalls_test"
  "ksPanelInitializeCalls_test"
  "ksPanelInputCalls_test"
  "ksPanelMasksCalls_test"
  "ksPanelMasksService_test"
  "ksPanelMatrixCalls_test"
  "ksPanelMonitorService_test"
  "ksPanelPedalOutputService_test"
  "ksPanelPercentCalls_test"
  "ksPanelPollCalls_test"
  "ksPanelPublishCalls_test"
  "ksPanelScanService_test"
  "ksPanelSettingsCalls_test"
  "ksPanelSettingsService_test"
  "ksPanelTextCalls_test"
  "ksPanelToggleCalls_test"
  "ksPanelVersionCalls_test"
  "ksParameterCurveCalls_test"
  "ksParameterFilterCalls_test"
  "ksParameterGateCalls_test"
  "ksParameterHeaderCalls_test"
  "ksParameterLfoCalls_test"
  "ksParameterModulationCalls_test"
  "ksParameterPitchCalls_test"
  "ksParameterScalarCalls_test"
  "ksParameterScaleCalls_test"
  "ksParameterSignedSquareCalls_test"
  "ksParameterSupportCalls_test"
  "ksParameterTimeCalls_test"
  "ksParameterUploadService_test"
  "ksParameterVoiceScaleCalls_test"
  "ksParameterVolumeCalls_test"
  "ksPartControlCalls_test"
  "ksPartEnableCalls_test"
  "ksPartFeedbackServiceTask_test"
  "ksPartFeedbackService_test"
  "ksPartModulationCalls_test"
  "ksPartNoteCalls_test"
  "ksPartRefreshCalls_test"
  "ksPartRouteCalls_test"
  "ksPartSignalCalls_test"
  "ksPatchExpandCalls_test"
  "ksPedalCalls_test"
  "ksPedalSenseCalls_test"
  "ksPerformanceSilence_test"
  "ksPeriodicCalls_test"
  "ksPeriodicService_test"
  "ksResetImageCalls_test"
  "ksResetVector_test"
  "ksRuntimeController_test"
  "ksScheduledTimerCalls_test"
  "ksScheduledTimerServiceTask_test"
  "ksScheduledTimerService_test"
  "ksSerialDirectCalls_test"
  "ksSerialForegroundService_test"
  "ksSerialIrqCalls_test"
  "ksSerialQueueCalls_test"
  "ksSerialReceiveCalls_test"
  "ksSerialReceiveService_test"
  "ksSerialSetupCalls_test"
  "ksSerialTransmitCalls_test"
  "ksServiceCycleCalls_test"
  "ksServiceCycleServiceTask_test"
  "ksServiceCycleService_test"
  "ksSquareRootCalls_test"
  "ksStartupBannerCalls_test"
  "ksStateApplyService_test"
  "ksStateAuditionCalls_test"
  "ksStateBankDisplayCalls_test"
  "ksStateBankNavigationCalls_test"
  "ksStateBrowseWorkflowCalls_test"
  "ksStateButtonDispatchCalls_test"
  "ksStateCentralActionsServiceTask_test"
  "ksStateCentralActionsService_test"
  "ksStateChannelCalls_test"
  "ksStateChannelService_test"
  "ksStateDeferredPartsCalls_test"
  "ksStateDeferredPartsService_test"
  "ksStateDisplayService_test"
  "ksStateDspFactoryResetCalls_test"
  "ksStateDspFactoryTransferCalls_test"
  "ksStateDspFactoryUploadCalls_test"
  "ksStateEditActionService_test"
  "ksStateEditAddressCalls_test"
  "ksStateEditContextCalls_test"
  "ksStateEditLabelCalls_test"
  "ksStateEditWorkflowCalls_test"
  "ksStateEncoderService_test"
  "ksStateEventsService_test"
  "ksStateFactoryService_test"
  "ksStateFactoryServicesCalls_test"
  "ksStateFieldActionsCalls_test"
  "ksStateFieldMapCalls_test"
  "ksStateFieldService_test"
  "ksStateFlashCommitCalls_test"
  "ksStateForegroundCalls_test"
  "ksStateFunctionKeysCalls_test"
  "ksStateGlobalSaveCalls_test"
  "ksStateGlobalsCalls_test"
  "ksStateInitCalls_test"
  "ksStateManualEditCalls_test"
  "ksStateMenuActionsCalls_test"
  "ksStateMenuFormatCalls_test"
  "ksStateMenuService_test"
  "ksStateModeCalls_test"
  "ksStateMultiLoadCalls_test"
  "ksStateMultiSaveCalls_test"
  "ksStateMultiSerializeCalls_test"
  "ksStateNameCodecCalls_test"
  "ksStateNameEditorCalls_test"
  "ksStatePackFieldsCalls_test"
  "ksStatePackRefreshCalls_test"
  "ksStatePanelModesCalls_test"
  "ksStatePanelRefreshCalls_test"
  "ksStatePanelService_test"
  "ksStateParameterEditCalls_test"
  "ksStateParameterExpandCalls_test"
  "ksStatePartNavigationService_test"
  "ksStatePartSelectionCalls_test"
  "ksStatePatchLoadCalls_test"
  "ksStatePatchNameCalls_test"
  "ksStatePlaybackModeService_test"
  "ksStatePostEditCalls_test"
  "ksStatePresetAddressCalls_test"
  "ksStatePresetFormatCalls_test"
  "ksStatePresetSaveCalls_test"
  "ksStateProgramBrowseCalls_test"
  "ksStateProgramBrowserService_test"
  "ksStateProgramCalls_test"
  "ksStateProgramChangeCalls_test"
  "ksStateProgramDisplayCalls_test"
  "ksStateProgramEventCalls_test"
  "ksStateProgramLoadService_test"
  "ksStateRefreshService_test"
  "ksStateReloadPanelService_test"
  "ksStateReloadService_test"
  "ksStateReloadVoiceService_test"
  "ksStateResetService_test"
  "ksStateSaveService_test"
  "ksStateSceneEditService_test"
  "ksStateSceneService_test"
  "ksStateSceneToggleService_test"
  "ksStateScreenActionsCalls_test"
  "ksStateScreenCalls_test"
  "ksStateScreenEditCalls_test"
  "ksStateScreenService_test"
  "ksStateSingleLoadCalls_test"
  "ksStateStatusDisplayCalls_test"
  "ksStateStatusService_test"
  "ksStateStorageCalls_test"
  "ksStateStoreMenuService_test"
  "ksStateStoreService_test"
  "ksStateStoreServicesCalls_test"
  "ksStateStoreUiCalls_test"
  "ksStateStoredValueCalls_test"
  "ksStateTickService_test"
  "ksStateValueBoundsCalls_test"
  "ksStateVoiceRefreshService_test"
  "ksSyncValueCalls_test"
  "ksSysExActionCalls_test"
  "ksSysExDumpCalls_test"
  "ksSysExFrameCalls_test"
  "ksSysExNativeServiceTask_test"
  "ksSysExNativeService_test"
  "ksSystemServicesCalls_test"
  "ksTimerServiceCalls_test"
  "ksTimingTablesCalls_test"
  "ksTpu0Service_test"
  "ksUnisonCalls_test"
  "ksVelocityCurveCalls_test"
  "ksVelocityModulationCalls_test"
  "ksVoiceFeedbackCalls_test"
  "ksVoiceFeedbackService_test"
  "ksVoicePopulationCalls_test"
  "ksVoiceRefreshCalls_test"
  "ksVoiceReleaseCalls_test"
  "ksVoiceSelectionCalls_test"
  "ksVoiceStartCalls_test"
  "ksVoiceSweepCalls_test"
  "ksVoiceTableInitCalls_test"
  "ksVoiceTrackingCalls_test"
  "silverWorkflow_test"
  "silverController_test"
  "ksLiveControls_test"
  "ksPresetPitch_test"
  "ksClock_test"
)
set(KS_LEGACY_BOARD_COMPARISON_TARGETS
  "ksLiveControls_test"
  "ksPresetPitch_test"
  "ksClock_test"
  "ksBoardSilence_test"
  "ksPerformanceSilence_test"
  "silverController_test"
  "ksControllerProbe_test"
  "ksControllerBoot_test"
  "ksDspEquivalence_test"
  "ksCpuCost_test"
  "ksControllerUart_test"
  "ksInterruptHandoff_test"
  "ksInterruptTimeline_test"
  "silverWorkflow_test"
)

# A deferred call executes at root scope. Preserve these finite source lists
# explicitly; caller-directory variables are not lexical function captures.
foreach(list_name KS_NATIVE_CONTROLLER_CPP KS_COMPATIBILITY_CONTROLLER_CPP KS_LEGACY_COMPATIBILITY_TEST_TARGETS KS_LEGACY_BOARD_COMPARISON_TARGETS)
  set_property(GLOBAL PROPERTY "KS_PARTITION_${list_name}" "${${list_name}}")
endforeach()

function(_ks_partition_collect_targets directory output)
  get_property(result DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(child IN LISTS children)
    _ks_partition_collect_targets("${child}" child_targets)
    list(APPEND result ${child_targets})
  endforeach()
  set(${output} "${result}" PARENT_SCOPE)
endfunction()

function(_ks_partition_copy_compile_properties target)
  # The deferred target is created in the root directory, so explicitly retain
  # ksLib's compile environment (including standalone visibility/PIC settings).
  foreach(property CXX_STANDARD CXX_STANDARD_REQUIRED CXX_EXTENSIONS POSITION_INDEPENDENT_CODE CXX_VISIBILITY_PRESET C_VISIBILITY_PRESET VISIBILITY_INLINES_HIDDEN MSVC_RUNTIME_LIBRARY COMPILE_OPTIONS COMPILE_DEFINITIONS INCLUDE_DIRECTORIES COMPILE_FLAGS INTERPROCEDURAL_OPTIMIZATION)
    get_target_property(value ksLib ${property})
    if(NOT value MATCHES "-NOTFOUND$")
      set_property(TARGET ${target} PROPERTY ${property} "${value}")
    endif()
  endforeach()
endfunction()

function(_ks_partition_assert_native_closure target visited header_only)
  if(NOT TARGET "${target}")
    return()
  endif()
  get_target_property(alias "${target}" ALIASED_TARGET)
  if(alias)
    set(target "${alias}")
  endif()
  if(target IN_LIST visited)
    return()
  endif()
  list(APPEND visited "${target}")
  set(forbidden ksCompatibility_test ksBoardComparisonRegistration_test ksH8Reference)
  if(header_only)
    list(APPEND forbidden ksLib h8s)
  endif()
  if(target IN_LIST forbidden)
    message(FATAL_ERROR "Native target has a legacy dependency: ${visited}")
  endif()
  foreach(property SOURCES LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(values "${target}" ${property})
    if(values MATCHES "-NOTFOUND$")
      continue()
    endif()
    foreach(value IN LISTS values)
      foreach(bad IN LISTS forbidden)
        if(value STREQUAL bad OR value MATCHES "[:,>]${bad}([,>]|$)")
          message(FATAL_ERROR "Native target ${target} contains legacy source/link ${value}")
        endif()
      endforeach()
      if(NOT property STREQUAL "SOURCES" AND TARGET "${value}")
        _ks_partition_assert_native_closure("${value}" "${visited}" "${header_only}")
      endif()
    endforeach()
  endforeach()
endfunction()

function(ks_apply_controller_source_partition)
  get_property(ks_dir GLOBAL PROPERTY KS_PARTITION_DIRECTORY)
  if(NOT ks_dir OR NOT TARGET ksLib)
    message(FATAL_ERROR "Schedule the admitted KS source directory after creating ksLib")
  endif()
  get_property(applied GLOBAL PROPERTY KS_PARTITION_APPLIED)
  if(applied)
    message(FATAL_ERROR "KS source partition must execute exactly once, after all target declarations")
  endif()
  foreach(list_name KS_NATIVE_CONTROLLER_CPP KS_COMPATIBILITY_CONTROLLER_CPP KS_LEGACY_COMPATIBILITY_TEST_TARGETS KS_LEGACY_BOARD_COMPARISON_TARGETS)
    get_property(${list_name} GLOBAL PROPERTY "KS_PARTITION_${list_name}")
  endforeach()
  foreach(pure ksNativeControllerExecutor_test ksProductionController_test)
    if(pure IN_LIST KS_LEGACY_COMPATIBILITY_TEST_TARGETS OR pure IN_LIST KS_LEGACY_BOARD_COMPARISON_TARGETS)
      message(FATAL_ERROR "Pure target must never be registered as legacy: ${pure}")
    endif()
  endforeach()
  # Preserve the old architecture/Calls rejection assertions as an explicit
  # comparison-only guard. This executable intentionally has no reference
  # factory, H8 object or Board registration, so rejection remains observable.
  if(TARGET ksProductionController_test AND NOT TARGET ksLegacyCompatibilityGuard_test)
    if(NOT EXISTS "${ks_dir}/ksLegacyCompatibilityGuard_test.cpp")
      message(FATAL_ERROR "The retained legacy rejection fixture is required")
    endif()
    add_executable(ksLegacyCompatibilityGuard_test "${ks_dir}/ksLegacyCompatibilityGuard_test.cpp")
    _ks_partition_copy_compile_properties(ksLegacyCompatibilityGuard_test)
    target_compile_features(ksLegacyCompatibilityGuard_test PRIVATE cxx_std_17)
    add_test(NAME ksLegacyCompatibilityGuard COMMAND ksLegacyCompatibilityGuard_test)
  endif()
  get_target_property(old_sources ksLib SOURCES)
  get_target_property(source_directory ksLib SOURCE_DIR)
  foreach(source IN LISTS old_sources)
    if(source MATCHES "\\.cpp$")
      get_filename_component(path "${source}" ABSOLUTE BASE_DIR "${source_directory}")
      get_filename_component(name "${path}" NAME)
      if(NOT name IN_LIST KS_NATIVE_CONTROLLER_CPP AND NOT name IN_LIST KS_COMPATIBILITY_CONTROLLER_CPP)
        message(FATAL_ERROR "Unclassified ksLib translation unit: ${source}")
      endif()
      if(NOT path STREQUAL "${ks_dir}/${name}")
        message(FATAL_ERROR "KS source escaped the admitted source directory: ${source}")
      endif()
    endif()
  endforeach()
  set(native_paths)
  foreach(name IN LISTS KS_NATIVE_CONTROLLER_CPP)
    if(NOT EXISTS "${ks_dir}/${name}")
      message(FATAL_ERROR "Actual admitted native source is required: ${name}")
    endif()
    list(APPEND native_paths "${ks_dir}/${name}")
  endforeach()
  list(LENGTH native_paths native_count)
  if(NOT native_count EQUAL 10)
    message(FATAL_ERROR "Native KS partition must contain the actual ten translation units")
  endif()
  set_property(TARGET ksLib PROPERTY SOURCES "${native_paths}")
  _ks_partition_collect_targets("${CMAKE_SOURCE_DIR}" all_targets)
  set(legacy_targets)
  foreach(target IN LISTS KS_LEGACY_COMPATIBILITY_TEST_TARGETS)
    if(TARGET "${target}")
      list(APPEND legacy_targets "${target}")
    endif()
  endforeach()
  # TARGET_OBJECTS is only an object insertion; it does not propagate the
  # object library's link interface. Retain every existing H8 insertion and
  # link its real definitions explicitly, including early wrapper tests.
  foreach(target IN LISTS all_targets)
    get_target_property(sources "${target}" SOURCES)
    list(FIND sources "$<TARGET_OBJECTS:ksH8Reference>" has_h8_object)
    get_target_property(links "${target}" LINK_LIBRARIES)
    list(FIND links "ksH8Reference" has_h8_link)
    if(NOT has_h8_object EQUAL -1 OR NOT has_h8_link EQUAL -1)
      list(APPEND legacy_targets "${target}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES legacy_targets)
  if(legacy_targets)
    set(compatibility_paths)
    foreach(name IN LISTS KS_COMPATIBILITY_CONTROLLER_CPP)
      if(NOT EXISTS "${ks_dir}/${name}")
        message(FATAL_ERROR "Actual compatibility source is required: ${name}")
      endif()
      list(APPEND compatibility_paths "${ks_dir}/${name}")
    endforeach()
    add_library(ksCompatibility_test STATIC EXCLUDE_FROM_ALL ${compatibility_paths})
    _ks_partition_copy_compile_properties(ksCompatibility_test)
    target_link_libraries(ksCompatibility_test PUBLIC ksLib)
    target_compile_features(ksCompatibility_test PUBLIC cxx_std_17)
    foreach(target IN LISTS legacy_targets)
      target_link_libraries("${target}" PRIVATE ksCompatibility_test)
    endforeach()
    set(board_targets)
    foreach(target IN LISTS KS_LEGACY_BOARD_COMPARISON_TARGETS)
      if(TARGET "${target}")
        list(APPEND board_targets "${target}")
      endif()
    endforeach()
    if(board_targets)
      if(NOT TARGET ksH8Reference)
        message(FATAL_ERROR "Actual Board comparison requires the H8 test object")
      endif()
      add_library(ksBoardComparisonRegistration_test OBJECT EXCLUDE_FROM_ALL "${ks_dir}/LegacyBoardComparisonBackend_test.cpp")
      _ks_partition_copy_compile_properties(ksBoardComparisonRegistration_test)
      target_link_libraries(ksBoardComparisonRegistration_test PRIVATE ksCompatibility_test)
      target_compile_features(ksBoardComparisonRegistration_test PRIVATE cxx_std_17)
      foreach(target IN LISTS board_targets)
        # An explicit object insertion retains its pre-main registration.
        target_sources("${target}" PRIVATE $<TARGET_OBJECTS:ksBoardComparisonRegistration_test>)
        get_target_property(sources "${target}" SOURCES)
        list(FIND sources "$<TARGET_OBJECTS:ksH8Reference>" has_h8_object)
        if(has_h8_object EQUAL -1)
          target_sources("${target}" PRIVATE $<TARGET_OBJECTS:ksH8Reference>)
        endif()
        target_link_libraries("${target}" PRIVATE ksCompatibility_test)
      endforeach()
    endif()
  endif()
  if(TARGET ksLegacyCompatibilityGuard_test)
    get_target_property(guard_links ksLegacyCompatibilityGuard_test LINK_LIBRARIES)
    if(NOT guard_links STREQUAL "ksCompatibility_test")
      message(FATAL_ERROR "Legacy rejection guard must link only its explicit compatibility target")
    endif()
    get_target_property(guard_sources ksLegacyCompatibilityGuard_test SOURCES)
    if(guard_sources MATCHES "ksH8Reference|ksBoardComparisonRegistration_test")
      message(FATAL_ERROR "Legacy rejection guard must not acquire a reference factory")
    endif()
  endif()
  _ks_partition_assert_native_closure(ksLib "" FALSE)
  _ks_partition_assert_native_closure(ksProductionController_test "" FALSE)
  _ks_partition_assert_native_closure(ksNativeControllerExecutor_test "" TRUE)
  set_property(GLOBAL PROPERTY KS_PARTITION_APPLIED TRUE)
  set_property(GLOBAL PROPERTY KS_PARTITION_EFFECTIVE_LEGACY_TARGETS "${legacy_targets}")
  set_property(GLOBAL PROPERTY KS_PARTITION_EFFECTIVE_BOARD_TARGETS "${board_targets}")
endfunction()

function(ks_schedule_controller_source_partition ks_dir)
  get_filename_component(directory "${ks_dir}" REALPATH)
  get_property(previous GLOBAL PROPERTY KS_PARTITION_DIRECTORY)
  if(previous AND NOT previous STREQUAL directory)
    message(FATAL_ERROR "KS partition scheduled for two different source roots")
  endif()
  set_property(GLOBAL PROPERTY KS_PARTITION_DIRECTORY "${directory}")
  get_property(scheduled GLOBAL PROPERTY KS_PARTITION_SCHEDULED)
  if(NOT scheduled)
    set_property(GLOBAL PROPERTY KS_PARTITION_SCHEDULED TRUE)
    # No late-evaluated caller variable arguments cross this scope boundary.
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL ks_apply_controller_source_partition)
  endif()
endfunction()
