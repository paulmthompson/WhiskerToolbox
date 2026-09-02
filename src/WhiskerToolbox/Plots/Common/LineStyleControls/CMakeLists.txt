find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(LINE_STYLE_CONTROLS_SOURCES

        Core/LineStyleState.cpp
        Core/LineStyleState.hpp
        LineStyleControls.cpp
        LineStyleControls.hpp

)

add_library(LineStyleControls STATIC ${LINE_STYLE_CONTROLS_SOURCES})

set_target_properties(LineStyleControls PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(LineStyleControls PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

target_link_libraries(LineStyleControls PUBLIC CorePlotting)

target_include_directories(LineStyleControls PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For Plots/Common sibling access
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(LineStyleControls)

# Enable automoc for Qt
set_target_properties(LineStyleControls PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(LineStyleControls)