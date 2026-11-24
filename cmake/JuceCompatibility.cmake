# Patch a build-local source copy; never modify an external dependency checkout.
set(SILVER_JUCE_PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/private-juce")
if(NOT EXISTS "${SILVER_JUCE_PRIVATE}/CMakeLists.txt")
    file(COPY "${JUCE_SOURCE}/" DESTINATION "${SILVER_JUCE_PRIVATE}" PATTERN ".git" EXCLUDE)
endif()
set(JUCE_SOURCE "${SILVER_JUCE_PRIVATE}")

# The Standalone wrapper stores filterState separately from Silver-K settings.
# Use the same profile root for both stores when testing with SILVER_K_DATA.
set(standalone_source "${JUCE_SOURCE}/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp")
file(READ "${standalone_source}" standalone_text)
set(standalone_profile [=[        const auto silverProfile = SystemStats::getEnvironmentVariable ("SILVER_K_DATA", {});
        if (silverProfile.isNotEmpty())
            options.folderName = File (silverProfile).getFullPathName();
]=])
string(FIND "${standalone_text}" "${standalone_profile}" profile_position)
if(profile_position EQUAL -1)
    set(configure_storage "        appProperties.setStorageParameters (options);")
    string(FIND "${standalone_text}" "${configure_storage}" storage_position)
    if(storage_position EQUAL -1)
        message(FATAL_ERROR "Unrecognized JUCE Standalone settings setup")
    endif()
    string(REPLACE "${configure_storage}" "${standalone_profile}\n${configure_storage}"
        standalone_text "${standalone_text}")
    file(WRITE "${standalone_source}" "${standalone_text}")
endif()

if(APPLE)
    # VST3 kPrefetch requests realtime-quality audio at an arbitrary cadence,
    # including faster than realtime. Use the existing full-speed render path
    # so the emulation queue is not drained as if each call had a live deadline.
    # kRealtime keeps its nonblocking queue-consumer path. This changes no DSP
    # quality, guest clock, queue size, latency, or worker priority.
    set(vst3_source "${JUCE_SOURCE}/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp")
    file(READ "${vst3_source}" vst3_text)
    set(vst3_original "${vst3_text}")
    foreach(mode_source newSetup data)
        string(REPLACE "setNonRealtime (${mode_source}.processMode == Vst::kOffline)"
            "setNonRealtime (${mode_source}.processMode != Vst::kRealtime)" vst3_text "${vst3_text}")
        string(FIND "${vst3_text}" "setNonRealtime (${mode_source}.processMode != Vst::kRealtime)" mode_position)
        if(mode_position EQUAL -1)
            message(FATAL_ERROR "Unrecognized JUCE VST3 processing-mode adapter")
        endif()
    endforeach()
    if(NOT vst3_text STREQUAL vst3_original)
        file(WRITE "${vst3_source}" "${vst3_text}")
    endif()
    set(snapshot_source "${JUCE_SOURCE}/modules/juce_gui_basics/native/juce_Windowing_mac.mm")
    file(READ "${snapshot_source}" snapshot_text)
    string(FIND "${snapshot_text}" "CGImageRef screenShot = CGWindowListCreateImage" legacy_position)
    if(NOT legacy_position EQUAL -1)
        string(REGEX REPLACE "CGImageRef screenShot = CGWindowListCreateImage \\(CGRectNull,[^;]+;"
            "NSView* view = [nsWindow contentView];\n        NSBitmapImageRep* bitmapRep = [[view bitmapImageRepForCachingDisplayInRect: [view bounds]] retain];\n        [view cacheDisplayInRect: [view bounds] toBitmapImageRep: bitmapRep];" snapshot_text "${snapshot_text}")
        string(REPLACE "NSBitmapImageRep* bitmapRep = [[NSBitmapImageRep alloc] initWithCGImage: screenShot];" "" snapshot_text "${snapshot_text}")
        string(REPLACE "CGImageRelease (screenShot);" "" snapshot_text "${snapshot_text}")
        file(WRITE "${snapshot_source}" "${snapshot_text}")
    endif()
endif()
