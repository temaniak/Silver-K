# macOS compatibility: JUCE 7 window capture and VST3 processing modes.
# Patch a build-local source copy; never modify an external dependency checkout.
if(APPLE)
    set(SILVER_JUCE_PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/private-juce")
    if(NOT EXISTS "${SILVER_JUCE_PRIVATE}/CMakeLists.txt")
        file(COPY "${JUCE_SOURCE}/" DESTINATION "${SILVER_JUCE_PRIVATE}" PATTERN ".git" EXCLUDE)
    endif()
    set(JUCE_SOURCE "${SILVER_JUCE_PRIVATE}")
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
