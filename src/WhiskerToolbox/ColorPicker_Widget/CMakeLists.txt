

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

target_link_libraries(ColorPicker_Widget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui

    DataManager
)

# Include directories
target_include_directories(ColorPicker_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ColorPicker_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ColorPicker_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(ColorPicker_Widget PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(ColorPicker_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)