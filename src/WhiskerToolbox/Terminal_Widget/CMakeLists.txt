
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(reflectcpp CONFIG REQUIRED)

add_library(Terminal_Widget STATIC 
        TerminalWidget.hpp 
        TerminalWidget.cpp 
        TerminalWidget.ui
        TerminalWidgetState.hpp
        TerminalWidgetState.cpp
        TerminalWidgetStateData.hpp
        TerminalWidgetRegistration.hpp
        TerminalWidgetRegistration.cpp
)

set_target_properties(Terminal_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(Terminal_Widget 
    PUBLIC
        EditorState
        Qt6::Widgets 
        Qt6::Core 
        Qt6::Gui
        reflectcpp::reflectcpp
)

target_include_directories(Terminal_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(Terminal_Widget)

set_target_properties(Terminal_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(Terminal_Widget)