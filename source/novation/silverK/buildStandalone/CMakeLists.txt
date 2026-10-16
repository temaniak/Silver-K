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
target_sources(ksLib PRIVATE "${ROOT}/source/novation/ks/ksLib/ksRuntimeController.cpp" "${ROOT}/source/novation/ks/ksLib/ksRuntimeStartup.cpp")
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
