find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(RATE_ESTIMATION_CONTROLS_SOURCES
        EstimationMethodControls.hpp
        EstimationMethodControls.cpp
        ScalingModeControls.hpp
        ScalingModeControls.cpp
)

add_library(RateEstimationControls STATIC ${RATE_ESTIMATION_CONTROLS_SOURCES})

set_target_properties(RateEstimationControls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_target_properties(RateEstimationControls PROPERTIES
    AUTOUIC ON
    AUTOMOC ON
)

target_link_libraries(RateEstimationControls PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

# EventRateEstimation provides the data types (EstimationParams, ScalingMode)
target_link_libraries(RateEstimationControls PUBLIC EventRateEstimation)

target_include_directories(RateEstimationControls PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"         # This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>" # WhiskerToolbox/ root
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(RateEstimationControls)

enable_whiskertoolbox_shared_pch(RateEstimationControls)