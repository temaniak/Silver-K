add_library(ksPhysicalDsp STATIC
    "${CMAKE_CURRENT_LIST_DIR}/ksDSP.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ksDspWorkers.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ksPhysicalDspPair.cpp")
target_compile_features(ksPhysicalDsp PUBLIC cxx_std_17)
target_link_libraries(ksPhysicalDsp PUBLIC baseLib dsp56kEmu)
option(SILVER_KS_DSP_JIT_OPTIMIZER "Enable the host-code optimizer for KS DSP blocks" OFF)
if(NOT SILVER_KS_DSP_JIT_OPTIMIZER)
    target_compile_definitions(ksPhysicalDsp PRIVATE SILVER_KS_DSP_JIT_NO_OPTIMIZER=1)
endif()
