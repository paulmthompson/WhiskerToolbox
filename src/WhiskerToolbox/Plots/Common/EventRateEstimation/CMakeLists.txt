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
target_link_libraries(EventRateEstimation PUBLIC TransformsV2)

target_include_directories(EventRateEstimation PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"         # This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>" # WhiskerToolbox/ root
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(EventRateEstimation)

enable_whiskertoolbox_shared_pch(EventRateEstimation)