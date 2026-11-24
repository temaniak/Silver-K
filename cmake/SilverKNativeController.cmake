find_package(Threads REQUIRED)
include("${ROOT}/source/novation/ks/ksLib/ksPhysicalDsp.cmake")

add_library(silverCommandScheduler STATIC
    "${ROOT}/source/novation/controller/command/commandScheduler.cpp"
    "${ROOT}/source/novation/controller/command/commandSession.cpp")
target_compile_features(silverCommandScheduler PUBLIC cxx_std_17)

add_library(silverCommandHdi STATIC "${ROOT}/source/novation/controller/command/dspWordTransport.cpp")
target_link_libraries(silverCommandHdi PUBLIC silverCommandScheduler)

add_library(silverCommandDspBoot STATIC
    "${ROOT}/source/novation/controller/command/dspBoot.cpp"
    "${ROOT}/source/novation/controller/command/controllerArchive.cpp")
target_link_libraries(silverCommandDspBoot PUBLIC silverCommandHdi PRIVATE baseLib)

add_library(silverCommandAudioHost STATIC "${ROOT}/source/novation/controller/command/commandAudioDevice.cpp")
target_compile_features(silverCommandAudioHost PUBLIC cxx_std_17)
target_link_libraries(silverCommandAudioHost PRIVATE baseLib dsp56kBase Threads::Threads)

add_library(silverKCommandFeedback STATIC
    "${ROOT}/source/novation/kstation/kstationLib/command/kCommandVoiceFeedback.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/command/kExpressionPacket.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/command/kPolyNoteImage.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/command/kCommandNoteOn.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/command/kCommandModWheelUpdate.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/command/kCommandNoteOff.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/command/kCommandLiveUpdate.cpp")
target_link_libraries(silverKCommandFeedback PUBLIC silverCommandHdi)

add_library(silverKsCommandRefresh STATIC
    "${ROOT}/source/novation/ks/ksLib/command/ksCommandGlobals.cpp"
    "${ROOT}/source/novation/ks/ksLib/command/ksCommandVoiceImage.cpp"
    "${ROOT}/source/novation/ks/ksLib/command/ksCommandParameterImage.cpp"
    "${ROOT}/source/novation/ks/ksLib/command/ksParameterIngress.cpp"
    "${ROOT}/source/novation/ks/ksLib/command/ksCommandRefresh.cpp")
target_link_libraries(silverKsCommandRefresh PUBLIC silverCommandHdi)

add_library(k_command_only STATIC
    "${ROOT}/source/novation/kstation/kstationLib/kPhysicalTimeline.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/kstationDSP.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/kstationRomLoader.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/command/kCommandColdStart.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/command/kCommandStartup.cpp"
    "${ROOT}/source/novation/kstation/kstationLib/command/kCommandDevice.cpp")
target_compile_definitions(k_command_only PRIVATE
    SILVER_K_PHYSICAL_STATIC=1 SILVER_K_ROM_EXPLICIT_ONLY=1 SILVER_K_COMMAND_HOST=1)
target_include_directories(k_command_only PUBLIC "${ROOT}/source/novation/kstation")
target_link_libraries(k_command_only PUBLIC silverCommandAudioHost
    PRIVATE silverKCommandFeedback silverCommandDspBoot dsp56kEmu baseLib)

add_library(ks_command_only STATIC
    "${ROOT}/source/novation/ks/ksLib/command/ksCommandRuntime.cpp"
    "${ROOT}/source/novation/ks/ksLib/command/ksCommandStartup.cpp"
    "${ROOT}/source/novation/ks/ksLib/command/ksCommandColdStart.cpp"
    "${ROOT}/source/novation/ks/ksLib/ksRom.cpp"
    "${ROOT}/source/novation/ks/ksLib/command/ksCommandDevice.cpp")
target_compile_definitions(ks_command_only PRIVATE SILVER_KS_NATIVE_COMMAND_ONLY=1 SILVER_KS_COMMAND_HOST=1)
target_include_directories(ks_command_only PUBLIC "${ROOT}/source/novation/ks")
target_link_libraries(ks_command_only PUBLIC silverCommandAudioHost
    PRIVATE ksPhysicalDsp silverKsCommandRefresh silverCommandDspBoot)

set_target_properties(ksPhysicalDsp silverCommandScheduler silverCommandHdi silverCommandDspBoot
    silverCommandAudioHost silverKCommandFeedback silverKsCommandRefresh k_command_only ks_command_only
    PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN ON EXCLUDE_FROM_ALL YES)
