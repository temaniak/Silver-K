# Every standalone entry point must set these before its first project().
# An existing empty cache value must not silently select the build machine's OS.
include_guard(GLOBAL)
if(CMAKE_HOST_APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "Minimum macOS version" FORCE)
    endif()
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
    endif()
endif()
