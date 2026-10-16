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

target_sources(ksLib PRIVATE ksRuntimeStartup.cpp ksRuntimeController.cpp ksRuntimeIo.cpp ksRuntimeMidi.cpp ksRuntimeState.cpp ksRuntimeParameters.cpp ksRuntimeVoices.cpp ksRuntimeExtended.cpp ksRuntimeController.h ksRuntimeBoundary.h)

target_sources(ksLib PRIVATE ksDspWordCalls.cpp ksDspWordCalls.h ksDspWordTask.h)
