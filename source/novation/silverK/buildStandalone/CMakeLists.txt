cmake_minimum_required(VERSION 3.22)
project(SilverK VERSION 0.2.4 LANGUAGES C CXX)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "Deployment target")
enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(BUILD_TESTING OFF CACHE BOOL "Build shared dependency tests" FORCE)
get_filename_component(GEARMULATOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(ROOT "${GEARMULATOR_ROOT}")
include("${ROOT}/cmake/Dependencies.cmake")
include("${ROOT}/cmake/JuceCompatibility.cmake")
include("${ROOT}/cmake/DspCompatibility.cmake")
add_subdirectory("${JUCE_SOURCE}" juce EXCLUDE_FROM_ALL)
set(JUCE_MODULES_DIR "${JUCE_SOURCE}/modules")
set(ASMJIT_STATIC TRUE CACHE BOOL "Static AsmJit" FORCE)
set(DSP56300_DEBUGGER OFF CACHE BOOL "DSP debugger" FORCE)
add_subdirectory("${ASMJIT_SOURCE}" asmjit EXCLUDE_FROM_ALL)
if(WIN32 OR (UNIX AND NOT APPLE))
    add_subdirectory("${DSP_SOURCE}/source/vtuneSdk" vtune EXCLUDE_FROM_ALL)
endif()
add_subdirectory("${DSP_SOURCE}/source/dsp56kBase" dspbase EXCLUDE_FROM_ALL)
add_subdirectory("${DSP_SOURCE}/source/dsp56kEmu" dspemu EXCLUDE_FROM_ALL)
add_subdirectory("${DSP_SOURCE}/source/dsp56kTestRunner" dspTests)
if(APPLE)
    target_link_libraries(dsp56kBase PUBLIC "-framework CoreAudio" "-framework AudioToolbox")
endif()
add_subdirectory("${GEARMULATOR_ROOT}/source/framework/baseLib" base)
add_subdirectory("${GEARMULATOR_ROOT}/source/3rdparty/libresample" resample)

# Existing dependency CMake generates version/buildconfig headers in its source
# directory. Private copies keep standalone builds out of the shared checkout.
set(PRIVATE_FRAMEWORK "${CMAKE_CURRENT_BINARY_DIR}/private-framework")
file(MAKE_DIRECTORY "${PRIVATE_FRAMEWORK}/juce")
file(COPY "${GEARMULATOR_ROOT}/source/framework/synthLib" DESTINATION "${PRIVATE_FRAMEWORK}")
file(COPY "${GEARMULATOR_ROOT}/source/framework/juce/jucePluginLib" DESTINATION "${PRIVATE_FRAMEWORK}/juce")
add_subdirectory("${PRIVATE_FRAMEWORK}/synthLib" synth)
target_include_directories(synthLib PUBLIC "${GEARMULATOR_ROOT}/source/framework" "${PRIVATE_FRAMEWORK}/synthLib")
add_subdirectory("${GEARMULATOR_ROOT}/source/3rdparty/ptypes" ptypes)
add_subdirectory("${GEARMULATOR_ROOT}/source/framework/networkLib" network)
add_subdirectory("${GEARMULATOR_ROOT}/source/framework/tools/bridge/bridgeLib" bridge)
add_subdirectory("${GEARMULATOR_ROOT}/source/framework/tools/bridge/client" bridgeClient)
add_subdirectory("${GEARMULATOR_ROOT}/source/cpu/h8s" h8s)

# Limit this standalone build to AU/VST3/Standalone; no unrelated synth targets.
set(SilverK_BUILD_JUCEPLUGIN_CLAP OFF CACHE BOOL "CLAP" FORCE)
set(SilverK_BUILD_JUCEPLUGIN_VST2 OFF CACHE BOOL "VST2" FORCE)
set(SilverK_BUILD_JUCEPLUGIN_Standalone ON CACHE BOOL "Standalone" FORCE)
set(TUS_CAN_RUN_BUILT_BINARIES FALSE)
include("${GEARMULATOR_ROOT}/source/cmake/juce.cmake")
add_subdirectory("${GEARMULATOR_ROOT}/source/framework/juce/juceUiLib" ui)
add_subdirectory("${PRIVATE_FRAMEWORK}/juce/jucePluginLib" pluginFramework)
target_include_directories(jucePluginLib PUBLIC "${GEARMULATOR_ROOT}/source/framework/juce" "${PRIVATE_FRAMEWORK}/juce/jucePluginLib")
add_subdirectory("${ROOT}/source/novation/kstation/kstationLib" kstationLib)
add_library(ksLib STATIC "${ROOT}/source/novation/ks/ksLib/ksBoard.cpp" "${ROOT}/source/novation/ks/ksLib/ksDSP.cpp" "${ROOT}/source/novation/ks/ksLib/ksDevice.cpp" "${ROOT}/source/novation/ks/ksLib/ksRom.cpp" "${ROOT}/source/novation/ks/ksLib/ksWorkerPlacement.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksControllerState.cpp" "${ROOT}/source/novation/ks/ksLib/ksKeySearchCalls.cpp" "${ROOT}/source/novation/ks/ksLib/ksParameterScaleCalls.cpp" "${ROOT}/source/novation/ks/ksLib/ksDspWordCalls.cpp" "${ROOT}/source/novation/ks/ksLib/ksDspAddressCalls.cpp" "${ROOT}/source/novation/ks/ksLib/ksDspFaultCalls.cpp" "${ROOT}/source/novation/ks/ksLib/ksDspBootStreamCalls.cpp" "${ROOT}/source/novation/ks/ksLib/ksDspBootSetupCalls.cpp")
target_include_directories(ksLib PUBLIC "${ROOT}/source/novation/ks" "${ROOT}/source/cpu" "${ROOT}/source/framework")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksDspWorkers.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksControllerTrace.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksDspTransport.cpp" "${ROOT}/source/novation/ks/ksLib/ksControllerJobs.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksLiveMidiController.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksLiveNoteController.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksLiveVoiceController.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksNativeInterrupts.cpp" "${ROOT}/source/novation/ks/ksLib/ksInterruptService.h" "${ROOT}/source/novation/ks/ksLib/ksInterruptScheduler.h" "${ROOT}/source/novation/ks/ksLib/ksTpu0Service.h" "${ROOT}/source/novation/ks/ksLib/ksAdcService.h" "${ROOT}/source/novation/ks/ksLib/ksSerialReceiveService.h" "${ROOT}/source/novation/ks/ksLib/ksRuntimeController.cpp" "${ROOT}/source/novation/ks/ksLib/ksRuntimeStartup.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksRuntimeIo.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksRuntimeMidi.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksRuntimeState.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksRuntimeParameters.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksRuntimeVoices.cpp")
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksRuntimeExtended.cpp")

target_link_libraries(ksLib PUBLIC baseLib dsp56kEmu)
# Frequency tracing is a separate diagnostic build; no per-boundary profile
# checks or storage are present in an ordinary Release controller.
option(SILVER_KS_CONTROLLER_PROFILING "Compile KS boundary-frequency diagnostics" OFF)
if(SILVER_KS_CONTROLLER_PROFILING)
 target_compile_definitions(ksLib PUBLIC SILVER_KS_CONTROLLER_PROFILING=1)
endif()
# Opt-in checks require user-supplied KS OS, zero wave bank and Flash paths.
add_executable(ksPartMidi_test "${ROOT}/source/novation/ks/ksLib/ksPartMidi_test.cpp")
target_compile_features(ksPartMidi_test PRIVATE cxx_std_17)
add_test(NAME ks_part_midi COMMAND ksPartMidi_test)
foreach(check ksBoardSilence ksPerformanceSilence)
    add_executable(${check}_test EXCLUDE_FROM_ALL "${ROOT}/source/novation/ks/ksLib/${check}_test.cpp")
    target_link_libraries(${check}_test PRIVATE ksLib)
endforeach()
add_subdirectory(../plugin silverPlugin)

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksDspStartupCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksDspRoutingCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPartControlCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVelocityCurveCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVoiceSelectionCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVoicePopulationCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksKeyResetCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVoiceReleaseCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksUnisonCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVoiceTrackingCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVelocityModulationCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlideCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVoiceStartCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksKeyActivityCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksNoteOnStateCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksOscillatorModulationCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalModulationCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVoiceSweepCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksKeyPriorityCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksScheduledTimerCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksOscillatorPitchCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterPitchCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterModulationCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVoiceRefreshCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalSetupCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksNoteOffStateCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterScalarCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalScalarCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalTimeCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksServiceCycleCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterTimeCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterCurveCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterVoiceScaleCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalShapeCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalRangeCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPartNoteCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPedalCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksNoteProgramCalls.cpp")


target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateInitCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterHeaderCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterSignedSquareCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterFilterCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterVolumeCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterGateCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterSupportCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksParameterLfoCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalMixCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalTableCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSquareRootCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalMatrixCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalClampedTimeCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalPairCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalTailScalarCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalTimeTailCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalFinishScalarCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalFinalCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMidiDecodeCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMidiControlCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMidiAuxCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksHardwareSetupCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMidiPartInitCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMidiRouteCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateFieldMapCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksGlobalEffectCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPartModulationCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPartRouteCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVoiceFeedbackCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksVoiceTableInitCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateParameterEditCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePatchNameCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateStorageCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPartEnableCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPartSignalCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelDecayCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateModeCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateParameterExpandCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePackFieldsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePackRefreshCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateGlobalsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPeriodicCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSyncValueCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPartRefreshCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelByteCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelPollCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelActionCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksArpPrimitiveCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksArpScheduleCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksArpMainCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateMultiSerializeCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePatchLoadCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateProgramCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateMultiLoadCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePresetAddressCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateSingleLoadCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSerialQueueCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSerialReceiveCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSerialSetupCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSerialTransmitCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSerialIrqCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSerialDirectCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksArpEventCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksArpSelectCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksResetImageCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateManualEditCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePostEditCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelControlCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelToggleCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelDecimalCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelTextCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksInternalMidiCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksInternalNoteCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSysExFrameCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksExceptionReturnCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateFlashCommitCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateStoreServicesCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePresetFormatCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePresetSaveCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateGlobalSaveCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateFactoryServicesCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelFormatCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelHexCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelPercentCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelVersionCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPatchExpandCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSysExActionCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksLegacySysExCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateMultiSaveCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateNameCodecCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateDeferredPartsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePanelRefreshCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateAuditionCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelPublishCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelMasksCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelInitializeCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelDetectCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksFirmwareLayoutCheckCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSysExDumpCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMidiFileTimeCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateProgramChangeCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateNameEditorCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateBankDisplayCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateDspFactoryTransferCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateDspFactoryResetCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateDspFactoryUploadCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksTimingTablesCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMidiFileEventCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMonoArpCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMidiFileTransportCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksEditAddressCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksEditSceneCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksEditDecisionCalls.cpp")

# Native Flash page comparison/programming; original bodies are test-only.
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksFlashPageCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksTimerServiceCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateChannelCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelInputCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateEditAddressCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateStoreUiCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelMatrixCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPedalSenseCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelEncoderCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelAdcSamplesCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelAdcScaleCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelAdcCalibrationCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelAdcIrqCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelAdcScanCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksInternalMidiQueueCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMidiOutputCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksInternalMidiServiceCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateForegroundCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateButtonDispatchCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelAnalogCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksClockSyncCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksClockPulseCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksClockDisplayCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateScreenCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelSettingsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateEditContextCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateScreenEditCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateMenuFormatCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSystemServicesCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksKeyboardEventsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePartSelectionCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateMenuActionsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksKeyboardScanCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksDiagnosticCodecCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksDiagnosticTransferCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksMemoryDiagnosticsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStartupBannerCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateScreenActionsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePanelModesCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateFunctionKeysCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateEditWorkflowCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksLoaderInputsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksLoaderCodecCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksLoaderPacketsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksLoaderProgramCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksLoaderSessionCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateValueBoundsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateEditLabelCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateFieldActionsCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateBankNavigationCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateStatusDisplayCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateStoredValueCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateBrowseWorkflowCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateProgramBrowseCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateProgramDisplayCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateProgramEventCalls.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksSerialForegroundService.h" "${ROOT}/source/novation/ks/ksLib/ksSerialForeground.h" "${ROOT}/source/novation/ks/ksLib/ksSerialForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateResetService.h" "${ROOT}/source/novation/ks/ksLib/ksStateResetForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateResetForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelScanService.h" "${ROOT}/source/novation/ks/ksLib/ksPanelScanForeground.h" "${ROOT}/source/novation/ks/ksLib/ksPanelScanForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelControlsService.h" "${ROOT}/source/novation/ks/ksLib/ksPanelControlsForeground.h" "${ROOT}/source/novation/ks/ksLib/ksPanelControlsForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateEventsService.h" "${ROOT}/source/novation/ks/ksLib/ksStateEventsForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateEventsForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStatePanelService.h" "${ROOT}/source/novation/ks/ksLib/ksStatePanelForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStatePanelForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateMenuService.h" "${ROOT}/source/novation/ks/ksLib/ksStateMenuForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateMenuForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateStoreService.h" "${ROOT}/source/novation/ks/ksLib/ksStateStoreForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateStoreForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateDisplayService.h" "${ROOT}/source/novation/ks/ksLib/ksStateDisplayForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateDisplayForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateApplyService.h" "${ROOT}/source/novation/ks/ksLib/ksStateApplyForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateApplyForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateSaveService.h" "${ROOT}/source/novation/ks/ksLib/ksStateSaveForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateSaveForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateReloadVoiceService.h" "${ROOT}/source/novation/ks/ksLib/ksStateReloadPanelService.h" "${ROOT}/source/novation/ks/ksLib/ksStateReloadCoordinator.h" "${ROOT}/source/novation/ks/ksLib/ksStateReloadService.h")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksPanelSettingsService.h" "${ROOT}/source/novation/ks/ksLib/ksPanelSettingsForeground.h" "${ROOT}/source/novation/ks/ksLib/ksPanelSettingsForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateScreenService.h" "${ROOT}/source/novation/ks/ksLib/ksStateScreenForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateScreenForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateFieldService.h" "${ROOT}/source/novation/ks/ksLib/ksStateFieldForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateFieldForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateUiRefreshService.h" "${ROOT}/source/novation/ks/ksLib/ksStateUiRefreshAbi.h")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksControllerDspService.h" "${ROOT}/source/novation/ks/ksLib/ksControllerDspAbi.h" "${ROOT}/source/novation/ks/ksLib/ksControllerCoefficientService.h" "${ROOT}/source/novation/ks/ksLib/ksControllerCoefficientAbi.h" "${ROOT}/source/novation/ks/ksLib/ksParameterUploadRecipe.h" "${ROOT}/source/novation/ks/ksLib/ksParameterUploadService.h" "${ROOT}/source/novation/ks/ksLib/ksParameterUploadForeground.h" "${ROOT}/source/novation/ks/ksLib/ksParameterUploadForeground.cpp" "${ROOT}/source/novation/ks/ksLib/ksGlobalUpdateService.h" "${ROOT}/source/novation/ks/ksLib/ksGlobalUpdateForeground.h" "${ROOT}/source/novation/ks/ksLib/ksGlobalUpdateForeground.cpp" "${ROOT}/source/novation/ks/ksLib/ksVoiceTrackingService.h" "${ROOT}/source/novation/ks/ksLib/ksStateVoiceRefreshService.h" "${ROOT}/source/novation/ks/ksLib/ksStateVoiceRefreshForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateVoiceRefreshForeground.cpp" "${ROOT}/source/novation/ks/ksLib/ksStateSceneEditService.h" "${ROOT}/source/novation/ks/ksLib/ksStateSceneEditForeground.h")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateSceneService.h" "${ROOT}/source/novation/ks/ksLib/ksStateSceneForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateSceneForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateSceneEditService.h" "${ROOT}/source/novation/ks/ksLib/ksStateSceneEditForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateSceneEditForeground.cpp")

target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksStateSaveAbi.h" "${ROOT}/source/novation/ks/ksLib/ksStateSceneToggleService.h" "${ROOT}/source/novation/ks/ksLib/ksStateSceneToggleForeground.h" "${ROOT}/source/novation/ks/ksLib/ksStateSceneToggleForeground.cpp")
