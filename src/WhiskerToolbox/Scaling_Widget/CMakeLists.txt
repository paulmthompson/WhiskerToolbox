find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(SCALING_WIDGET_SOURCES
    Scaling_Widget.cpp
    Scaling_Widget.hpp
    Scaling_Widget.ui
)

add_library(Scaling_Widget STATIC ${SCALING_WIDGET_SOURCES})

target_link_libraries(Scaling_Widget
    PUBLIC
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
        NEURALYZER_GEOMETRY  # For ImageSize struct
)

target_include_directories(Scaling_Widget
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(Scaling_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Scaling_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Scaling_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(Scaling_Widget PRIVATE ${MSVC_WARNINGS})
endif()

add_library(WhiskerToolbox::Scaling_Widget ALIAS Scaling_Widget)
