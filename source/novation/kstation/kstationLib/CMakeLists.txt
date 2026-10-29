cmake_minimum_required(VERSION 3.15)
project(kstationLib)

add_library(kstationLib STATIC
    kstationDevice.cpp kstationDevice.h
    kstationDSP.cpp kstationDSP.h
    kstationH8.cpp kstationH8.h
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
    foreach(check AdcService MainLoopTask MidiPumpTask DspTransferTask PatchUnpackTask ResetImageTask DspStartupTask FlashStateTask MidiEventTask VoiceAllocationTask)
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
