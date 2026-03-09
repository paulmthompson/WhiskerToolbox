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

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(RateEstimationControls PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(RateEstimationControls PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(RateEstimationControls PRIVATE ${MSVC_WARNINGS})
endif()
enable_whiskertoolbox_pch(RateEstimationControls)