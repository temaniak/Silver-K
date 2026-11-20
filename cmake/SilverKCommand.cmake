find_package(Threads REQUIRED)
include("${CMAKE_CURRENT_SOURCE_DIR}/source/novation/ks/ksLib/ksPhysicalDsp.cmake")

add_library(silverCommandScheduler STATIC
    source/novation/controller/command/commandScheduler.cpp
    source/novation/controller/command/commandSession.cpp)
target_compile_features(silverCommandScheduler PUBLIC cxx_std_17)

add_library(silverCommandHdi STATIC source/novation/controller/command/dspWordTransport.cpp)
target_link_libraries(silverCommandHdi PUBLIC silverCommandScheduler)

add_library(silverCommandDspBoot STATIC
    source/novation/controller/command/dspBoot.cpp
    source/novation/controller/command/controllerArchive.cpp)
target_link_libraries(silverCommandDspBoot PUBLIC silverCommandHdi PRIVATE baseLib)

add_library(silverCommandAudioHost STATIC source/novation/controller/command/commandAudioDevice.cpp)
target_compile_features(silverCommandAudioHost PUBLIC cxx_std_17)
target_link_libraries(silverCommandAudioHost PRIVATE baseLib dsp56kBase Threads::Threads)

add_library(silverKCommandFeedback STATIC
    source/novation/kstation/kstationLib/command/kCommandVoiceFeedback.cpp
    source/novation/kstation/kstationLib/command/kExpressionPacket.cpp
    source/novation/kstation/kstationLib/command/kPolyNoteImage.cpp
    source/novation/kstation/kstationLib/command/kCommandNoteOn.cpp
    source/novation/kstation/kstationLib/command/kCommandModWheelUpdate.cpp
    source/novation/kstation/kstationLib/command/kCommandNoteOff.cpp
    source/novation/kstation/kstationLib/command/kCommandLiveUpdate.cpp)
target_link_libraries(silverKCommandFeedback PUBLIC silverCommandHdi)

add_library(silverKsCommandRefresh STATIC
    source/novation/ks/ksLib/command/ksCommandGlobals.cpp
    source/novation/ks/ksLib/command/ksCommandVoiceImage.cpp
    source/novation/ks/ksLib/command/ksCommandParameterImage.cpp
    source/novation/ks/ksLib/command/ksParameterIngress.cpp
    source/novation/ks/ksLib/command/ksCommandRefresh.cpp)
target_link_libraries(silverKsCommandRefresh PUBLIC silverCommandHdi)

add_library(k_command_only STATIC
    source/novation/kstation/kstationLib/kPhysicalTimeline.cpp
    source/novation/kstation/kstationLib/kstationDSP.cpp
    source/novation/kstation/kstationLib/kstationRomLoader.cpp
    source/novation/kstation/kstationLib/command/kCommandColdStart.cpp
    source/novation/kstation/kstationLib/command/kCommandStartup.cpp
    source/novation/kstation/kstationLib/command/kCommandDevice.cpp)
target_compile_definitions(k_command_only PRIVATE
    SILVER_K_PHYSICAL_STATIC=1 SILVER_K_ROM_EXPLICIT_ONLY=1 SILVER_K_COMMAND_HOST=1)
target_include_directories(k_command_only PUBLIC source/novation/kstation)
target_link_libraries(k_command_only PUBLIC silverCommandAudioHost
    PRIVATE silverKCommandFeedback silverCommandDspBoot dsp56kEmu baseLib)

add_library(ks_command_only STATIC
    source/novation/ks/ksLib/command/ksCommandRuntime.cpp
    source/novation/ks/ksLib/command/ksCommandStartup.cpp
    source/novation/ks/ksLib/command/ksCommandColdStart.cpp
    source/novation/ks/ksLib/ksRom.cpp
    source/novation/ks/ksLib/command/ksCommandDevice.cpp)
target_compile_definitions(ks_command_only PRIVATE SILVER_KS_NATIVE_COMMAND_ONLY=1 SILVER_KS_COMMAND_HOST=1)
target_include_directories(ks_command_only PUBLIC source/novation/ks)
target_link_libraries(ks_command_only PUBLIC silverCommandAudioHost
    PRIVATE ksPhysicalDsp silverKsCommandRefresh silverCommandDspBoot)

set_target_properties(ksPhysicalDsp silverCommandScheduler silverCommandHdi silverCommandDspBoot
    silverCommandAudioHost silverKCommandFeedback silverKsCommandRefresh k_command_only ks_command_only
    PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN ON EXCLUDE_FROM_ALL YES)

juce_add_plugin(silverKCommand VERSION 0.5.0 COMPANY_NAME "NK Stuff"
    BUNDLE_ID "com.theusualsuspects.silverk.command"
    PLUGIN_MANUFACTURER_CODE TusP PLUGIN_CODE SkcN PRODUCT_NAME "Silver-K Command"
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Silver-K Command uses the selected audio input for synthesis."
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE COPY_PLUGIN_AFTER_BUILD FALSE VST3_AUTO_MANIFEST FALSE
    FORMATS VST3 Standalone)
target_sources(silverKCommand PRIVATE
    source/novation/silverK/plugin/silverCommandProcessor.cpp
    source/novation/silverK/plugin/silverCommandEditor.cpp
    source/novation/silverK/plugin/silverKSData.cpp)
target_compile_features(silverKCommand PRIVATE cxx_std_17)
target_compile_definitions(silverKCommand PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(silverKCommand PRIVATE
    k_command_only ks_command_only baseLib juce::juce_audio_utils juce::juce_audio_plugin_client)
set_target_properties(silverKCommand silverKCommand_VST3 silverKCommand_Standalone PROPERTIES EXCLUDE_FROM_ALL YES)

if(APPLE)
    foreach(format VST3 Standalone)
        add_custom_command(TARGET silverKCommand_${format} POST_BUILD
            COMMAND /usr/bin/codesign --force --deep --sign - --timestamp=none
                    "$<TARGET_BUNDLE_DIR:silverKCommand_${format}>"
            VERBATIM)
    endforeach()
endif()

if(WIN32 AND SilverK_ASIO_SDK_PATH AND EXISTS "${SilverK_ASIO_SDK_PATH}/common/iasiodrv.h")
    target_compile_definitions(silverKCommand PUBLIC JUCE_ASIO=1)
    target_include_directories(silverKCommand PUBLIC "${SilverK_ASIO_SDK_PATH}/common")
endif()
if(MSVC)
    foreach(format VST3 Standalone)
        target_link_options(silverKCommand_${format} PRIVATE
            "/MAP:$<TARGET_FILE_DIR:silverKCommand_${format}>/silverKCommand_${format}.map")
    endforeach()
endif()
