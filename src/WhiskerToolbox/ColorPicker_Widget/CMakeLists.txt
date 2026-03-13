

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

set(COLOR_PICKER_WIDGET_SOURCES
    ColorPicker_Widget.hpp
    ColorPicker_Widget.cpp
    ColorPicker_Widget.ui
)

add_library(ColorPicker_Widget STATIC ${COLOR_PICKER_WIDGET_SOURCES})

set_target_properties(ColorPicker_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(ColorPicker_Widget PUBLIC 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(ColorPicker_Widget PRIVATE CoreUtilities)

# Include directories
target_include_directories(ColorPicker_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(ColorPicker_Widget)

set_target_properties(ColorPicker_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(ColorPicker_Widget)