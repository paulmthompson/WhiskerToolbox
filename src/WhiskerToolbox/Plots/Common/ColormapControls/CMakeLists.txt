find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(COLORMAP_CONTROLS_SOURCES
        ColormapControls.hpp
        ColormapControls.cpp
)

add_library(ColormapControls STATIC ${COLORMAP_CONTROLS_SOURCES})

set_target_properties(ColormapControls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(ColormapControls PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

# CorePlotting provides the colormap types (ColormapPreset, ColormapFunction, getLUT)
target_link_libraries(ColormapControls PUBLIC CorePlotting)

target_include_directories(ColormapControls PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"         # This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>" # WhiskerToolbox/ root
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ColormapControls PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ColormapControls PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(ColormapControls PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(ColormapControls PROPERTIES
    AUTOMOC ON
)
enable_whiskertoolbox_shared_pch(ColormapControls)