set(EVENT_RATE_ESTIMATION_SOURCES
        EventRateEstimation.hpp
        EventRateEstimation.cpp
        EstimationParams.hpp
        RateEstimate.hpp
        RateNormalization.hpp
        RateNormalization.cpp
        RateUncertainty.hpp
        RateUncertainty.cpp
)

add_library(EventRateEstimation STATIC ${EVENT_RATE_ESTIMATION_SOURCES})

set_target_properties(EventRateEstimation PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# DataManager, GatherResult, and PlotAlignmentWidget types all appear in the
# public API (function parameters/return types), so they are PUBLIC deps.
target_link_libraries(EventRateEstimation PUBLIC DataManager)
target_link_libraries(EventRateEstimation PUBLIC GatherResult)
target_link_libraries(EventRateEstimation PUBLIC PlotAlignmentWidget)

target_include_directories(EventRateEstimation PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"         # This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>" # WhiskerToolbox/ root
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(EventRateEstimation PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(EventRateEstimation PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(EventRateEstimation PRIVATE ${MSVC_WARNINGS})
endif()
