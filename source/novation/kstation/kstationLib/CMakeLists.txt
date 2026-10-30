cmake_minimum_required(VERSION 3.15)
project(kstationLib)

add_library(kstationLib STATIC
    kstationDevice.cpp kstationDevice.h
    kstationDSP.cpp kstationDSP.h
    kstationH8.cpp kstationH8.h
    kstationController.h kstationControllerTypes.h kstationNativeController.cpp kstationNativeController.h
    kstationHardware.cpp kstationHardware.h
    kstationHdiWait.h kstationNativeTimers.h kstationNativeIrq.h
    kstationSerialTxService.h kstationNativeSerialTx.h
    kstationClockPulseTask.h kstationClockTasks.h kstationNativeClock.h
    kstationTimer1Service.h kstationNativeTimer1.h
    kstationSerialRxService.h kstationNativeSerialRx.h
    kstationAdcService.h kstationNativeAdc.h
    kstationMainLoopTask.h kstationNativeMainLoop.h
    kstationMidiPumpTask.h kstationNativeMidiPump.h kstationNativeForeground.h
    kstationDspTransferTask.h kstationNativeDspTransfer.h
    kstationPatchUnpackTask.h kstationNativePatchUnpack.h
    kstationResetImageTask.h kstationNativeStartup.h
    kstationDspStartupTask.h kstationNativeDspStartup.h
    kstationFlashStateTask.h kstationLcdTask.h kstationNativeFlashState.h
    kstationMidiEventTask.h kstationNativeMidiEvent.h kstationNativeMidiEvents.h
    kstationVoiceAllocationTask.h kstationNativeVoiceAllocation.h
    kstationVoiceUpdateTask.h kstationNativeVoiceUpdate.h
    kstationQueue.h
    kstationRomLoader.cpp kstationRomLoader.h)
target_sources(kstationLib PRIVATE
    kstationArpMainTask.h
    kstationControllerAdcTask.h
    kstationControllerClockRuntime.h
    kstationControllerDeviceState.h
    kstationControllerDspTransferRuntime.h
    kstationControllerForegroundRuntime.h
    kstationControllerForegroundTask.h
    kstationControllerInterruptOwner.h
    kstationControllerInterruptRuntime.h
    kstationControllerInterruptScheduler.h
    kstationControllerInterruptTask.h
    kstationControllerMidiDequeueTask.h
    kstationControllerMidiDspTriggerTask.h
    kstationControllerMidiFirstNoteTask.h
    kstationControllerMidiHeldOffTask.h
    kstationControllerMidiHeldOnTask.h
    kstationControllerMidiNearestNoteTask.h
    kstationControllerMidiNoteRuntime.h
    kstationControllerMidiParserTask.h
    kstationControllerMidiPumpTask.h
    kstationControllerMidiTriggerTask.h
    kstationControllerMidiUtilityTask.h
    kstationControllerSerialRxTask.h
    kstationControllerSerialTxTask.h
    kstationControllerTimer1Task.h
    kstationControllerVoiceActivationTask.h
    kstationControllerVoiceBuildTask.h
    kstationControllerVoiceReleaseDspTask.h
    kstationControllerVoiceReleaseTask.h
    kstationControllerVoiceSelectionTask.h
    kstationControllerVoiceStartTask.h
    kstationDisplayNoticeTask.h
    kstationDspGlobalsTask.h
    kstationDspTriggerTask.h
    kstationFlashCommitTask.h
    kstationForegroundClockTask.h
    kstationKeyTask.h
    kstationLcdTextTask.h
    kstationMainUiTask.h
    kstationMetadataTask.h
    kstationMidiControlTask.h
    kstationMidiInitTask.h
    kstationMidiQueue.h
    kstationMidiSendControlTask.h
    kstationMidiSysExTask.h
    kstationNativeArp.h
    kstationNativeArpMain.h
    kstationNativeDevelopmentServices.h
    kstationNativeDisplayNotice.h
    kstationNativeDisplayNoticeOwner.h
    kstationNativeDspGlobals.h
    kstationNativeDspTrigger.h
    kstationNativeFlashCommit.h
    kstationNativeFlashCommitOwner.h
    kstationNativeForegroundClock.h
    kstationNativeKey.h
    kstationNativeKeyOwner.h
    kstationNativeLcdText.h
    kstationNativeMainUi.h
    kstationNativeMetadata.h
    kstationNativeMidiControl.h
    kstationNativeMidiInit.h
    kstationNativeMidiInitOwner.h
    kstationNativeMidiSendControl.h
    kstationNativeMidiSysEx.h
    kstationNativeMidiSysExOwner.h
    kstationNativeNumericDisplay.h
    kstationNativePanelHardware.h
    kstationNativePanelRefresh.h
    kstationNativeParameterEdit.h
    kstationNativeParameterInput.h
    kstationNativeParameterUpdate.h
    kstationNativePatchPack.h
    kstationNativePatchPackOwner.h
    kstationNativeProgramState.h
    kstationNativeProgramVoices.h
    kstationNativeStartupAdc.h
    kstationNativeStartupBanner.h
    kstationNativeStartupButtons.h
    kstationNativeStartupControl.h
    kstationNativeStartupDisplay.h
    kstationNativeStartupHardware.h
    kstationNativeStartupKeyboard.h
    kstationNativeStartupServices.h
    kstationNativeStartupState.h
    kstationNativeStartupTables.h
    kstationNativeTransposeApply.h
    kstationNativeTransposeApplyOwner.h
    kstationNativeUiEncoder.h
    kstationNativeUiServices.h
    kstationNativeVoiceEffects.h
    kstationNativeVoiceGlobal.h
    kstationNativeVoiceGlobalOwner.h
    kstationNativeVoiceSync.h
    kstationNumericDisplayTask.h
    kstationPanelHardwareTask.h
    kstationPanelRefreshTask.h
    kstationParameterEditTask.h
    kstationParameterInputTask.h
    kstationParameterUpdateTask.h
    kstationPatchPackTask.h
    kstationProgramStateTask.h
    kstationProgramVoicesTask.h
    kstationStartupAdcTask.h
    kstationStartupBannerTask.h
    kstationStartupButtonsTask.h
    kstationStartupControlTask.h
    kstationStartupDisplayTask.h
    kstationStartupHardwareTask.h
    kstationStartupKeyboardTask.h
    kstationStartupServicesTask.h
    kstationStartupStateTask.h
    kstationStartupTablesTask.h
    kstationStartupTask.h
    kstationTransposeApplyTask.h
    kstationUiEncoderTask.h
    kstationUiServicesTask.h
    kstationVoiceEffectsTask.h
    kstationVoiceGlobalTask.h
    kstationVoiceInitTask.h
    kstationVoiceSyncTask.h
)
target_sources(kstationLib PRIVATE
    kstationControllerBoard.h
    kstationControllerDspTriggerRuntime.h
    kstationControllerExpressionApplyTask.h
    kstationControllerExpressionValuesRuntime.h
    kstationControllerMidiControlRuntime.h
    kstationControllerMidiControlTask.h
    kstationControllerMidiExpressionTask.h
    kstationControllerMidiSysExDumpTask.h
    kstationControllerMidiSysExRuntime.h
    kstationControllerMidiSysExTask.h
    kstationControllerModulationDepthRuntime.h
    kstationControllerModulationPitchRuntime.h
    kstationControllerPatchUnpackTask.h
    kstationControllerProgramCopyTask.h
    kstationControllerProgramLoadTask.h
    kstationControllerProgramRuntime.h
    kstationControllerProgramTask.h
    kstationControllerTimers.h
    kstationControllerVoiceBipolarTask.h
    kstationControllerVoiceDepthLaneTask.h
    kstationControllerVoiceDspWordTask.h
    kstationControllerVoiceEnvelopeSequence.h
    kstationControllerVoiceExtraTasks.h
    kstationControllerVoiceFeedbackRuntime.h
    kstationControllerVoiceFinalSequence.h
    kstationControllerVoiceLinearTask.h
    kstationControllerVoiceLongFinal.h
    kstationControllerVoiceLongGlide.h
    kstationControllerVoiceLongPortamento.h
    kstationControllerVoiceLongPrefix.h
    kstationControllerVoiceLongTiming.h
    kstationControllerVoiceParameterStartTask.h
    kstationControllerVoicePitchLaneTask.h
    kstationControllerVoiceRefreshRuntime.h
    kstationControllerVoiceSlotScaleTask.h
    kstationControllerVoiceSourceSequence.h
    kstationControllerVoiceStatusTask.h
    kstationControllerVoiceSyncRuntime.h
    kstationDisplayFormatTask.h
    kstationMidiWrappersTask.h
    kstationNativeDisplayFormat.h
    kstationNativeDisplayFormatOwner.h
    kstationNativeMidiWrappers.h
    kstationNativeMidiWrappersOwner.h
    kstationNativePeriodicDisplay.h
    kstationNativeUiPanel.h
    kstationNativeVoiceFeedback.h
    kstationPeriodicDisplayTask.h
    kstationUiPanelTask.h
    kstationVoiceFeedbackTask.h
)
target_compile_features(kstationLib PUBLIC cxx_std_17)
target_link_libraries(kstationLib PUBLIC synthLib dsp56kEmu h8s)
target_include_directories(kstationLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_property(TARGET kstationLib PROPERTY FOLDER "Gearmulator")

if(BUILD_TESTING)
    add_executable(kstationRomLoader_test kstationRomLoader_test.cpp)
    target_link_libraries(kstationRomLoader_test PRIVATE kstationLib)
    # Firmware is external; run this executable with the three user ROM paths.
    add_executable(kstationQueue_test kstationQueue_test.cpp)
    target_link_libraries(kstationQueue_test PRIVATE kstationLib)
    add_test(NAME kstationQueue COMMAND kstationQueue_test)
endif()

option(KSTATION_BUILD_CONTROLLER_COMPLETION_CHECK "Build native K controller development differential checks" OFF)
if(KSTATION_BUILD_CONTROLLER_COMPLETION_CHECK)
    foreach(check AdcService MainLoopTask MidiPumpTask DspTransferTask PatchUnpackTask ResetImageTask DspStartupTask FlashStateTask MidiEventTask VoiceAllocationTask VoiceUpdateTask VoiceGlobalTask DspTriggerTask ParameterInputTask ParameterUpdateTask ParameterEditTask ControllerDeviceState ControllerDspTransferRuntime ControllerForegroundRuntime ControllerForegroundBinding ControllerForegroundParser ControllerInterruptRuntime ControllerInterruptOracle ControllerTimer1Runtime ControllerAdcRuntime ControllerClockRuntime ControllerTimers ControllerDspTriggerRuntime VoiceFeedbackTask)
        add_executable(kstation${check}_test kstation${check}_test.cpp)
        target_link_libraries(kstation${check}_test PRIVATE h8s)
        target_compile_features(kstation${check}_test PRIVATE cxx_std_17)
        add_test(NAME kstation_${check} COMMAND kstation${check}_test)
        set_tests_properties(kstation_${check} PROPERTIES SKIP_RETURN_CODE 77)
    endforeach()
    add_executable(kstationCompletionGuard_test kstationCompletionGuard_test.cpp)
    target_link_libraries(kstationCompletionGuard_test PRIVATE kstationLib)
    add_test(NAME kstation_controller_completion_guards COMMAND kstationCompletionGuard_test)
    set_tests_properties(kstation_controller_completion_guards PROPERTIES SKIP_RETURN_CODE 77)
endif()

option(KSTATION_BUILD_NATIVE_STAGE2_CHECK "Build native K IRQ differential checks" OFF)
if(KSTATION_BUILD_NATIVE_STAGE2_CHECK)
    add_executable(kstationNativeIrq_test kstationNativeIrq_test.cpp)
    target_link_libraries(kstationNativeIrq_test PRIVATE h8s)
    target_compile_features(kstationNativeIrq_test PRIVATE cxx_std_17)
    add_test(NAME kstation_native_irq COMMAND kstationNativeIrq_test)
    set_tests_properties(kstation_native_irq PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(kstationStage2Guard_test kstationStage2Guard_test.cpp)
    target_link_libraries(kstationStage2Guard_test PRIVATE kstationLib)
    add_test(NAME kstation_stage2_guards COMMAND kstationStage2Guard_test)
    set_tests_properties(kstation_stage2_guards PROPERTIES SKIP_RETURN_CODE 77)
endif()

option(KSTATION_BUILD_NATIVE_STAGE1_CHECK "Build original-H8/native stage-1 differential checks" OFF)
if(KSTATION_BUILD_NATIVE_STAGE1_CHECK)
    foreach(check NativeTimers HdiWait)
        add_executable(kstation${check}_test kstation${check}_test.cpp)
        target_link_libraries(kstation${check}_test PRIVATE h8s)
        target_compile_features(kstation${check}_test PRIVATE cxx_std_17)
    endforeach()
    add_test(NAME kstation_native_timers COMMAND kstationNativeTimers_test)
    add_test(NAME kstation_hdi_wait COMMAND kstationHdiWait_test)
    set_tests_properties(kstation_hdi_wait PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(kstationStage1Guard_test kstationStage1Guard_test.cpp)
    target_link_libraries(kstationStage1Guard_test PRIVATE kstationLib)
    add_test(NAME kstation_stage1_guards COMMAND kstationStage1Guard_test)
    set_tests_properties(kstation_stage1_guards PROPERTIES SKIP_RETURN_CODE 77)
endif()

option(KSTATION_BUILD_HARDWARE_CHECK "Build original reset/MIDI/audio firmware check" OFF)
if(KSTATION_BUILD_HARDWARE_CHECK)
    add_executable(kstationHardware_test kstationHardware_test.cpp)
    target_link_libraries(kstationHardware_test PRIVATE kstationLib)
    add_executable(kstationArp_test kstationArp_test.cpp)
    target_link_libraries(kstationArp_test PRIVATE kstationLib)
endif()

option(KSTATION_BUILD_NATIVE_STAGE3_CHECK "Build KS-style K serial TX differential checks" OFF)
if(KSTATION_BUILD_NATIVE_STAGE3_CHECK)
    add_executable(kstationSerialTxService_test kstationSerialTxService_test.cpp)
    target_link_libraries(kstationSerialTxService_test PRIVATE h8s)
    target_compile_features(kstationSerialTxService_test PRIVATE cxx_std_17)
    add_test(NAME kstation_serial_tx COMMAND kstationSerialTxService_test)
    set_tests_properties(kstation_serial_tx PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(kstationStage3Guard_test kstationStage3Guard_test.cpp)
    target_link_libraries(kstationStage3Guard_test PRIVATE kstationLib)
    add_test(NAME kstation_stage3_guards COMMAND kstationStage3Guard_test)
    set_tests_properties(kstation_stage3_guards PROPERTIES SKIP_RETURN_CODE 77)
endif()

option(KSTATION_BUILD_NATIVE_STAGE4_CHECK "Build K Timer1/RX/F8 differential checks" OFF)
if(KSTATION_BUILD_NATIVE_STAGE4_CHECK)
    foreach(check ClockService SerialRxService Timer1Service)
        add_executable(kstation${check}_test kstation${check}_test.cpp)
        target_link_libraries(kstation${check}_test PRIVATE h8s)
        target_compile_features(kstation${check}_test PRIVATE cxx_std_17)
        add_test(NAME kstation_${check} COMMAND kstation${check}_test)
        set_tests_properties(kstation_${check} PROPERTIES SKIP_RETURN_CODE 77)
    endforeach()
    add_executable(kstationStage4Guard_test kstationStage4Guard_test.cpp)
    target_link_libraries(kstationStage4Guard_test PRIVATE kstationLib)
    add_test(NAME kstation_stage4_guards COMMAND kstationStage4Guard_test)
    set_tests_properties(kstation_stage4_guards PROPERTIES SKIP_RETURN_CODE 77)
endif()
