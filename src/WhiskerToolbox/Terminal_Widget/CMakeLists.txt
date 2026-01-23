
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
    PRIVATE 
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

target_compile_definitions(Terminal_Widget PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Terminal_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Terminal_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(Terminal_Widget PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(Terminal_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
