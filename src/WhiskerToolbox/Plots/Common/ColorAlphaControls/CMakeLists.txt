find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(COLOR_ALPHA_CONTROLS_SOURCES
    ColorAlphaControls.cpp
    ColorAlphaControls.hpp
)

add_library(ColorAlphaControls STATIC ${COLOR_ALPHA_CONTROLS_SOURCES})

set_target_properties(ColorAlphaControls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(ColorAlphaControls PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

target_include_directories(ColorAlphaControls PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(ColorAlphaControls)

set_target_properties(ColorAlphaControls PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(ColorAlphaControls)
