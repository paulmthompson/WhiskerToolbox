
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

# Define the Collapsible_Widget library sources
set(COLLAPSIBLE_WIDGET_SOURCES
    Section.cpp
    Section.hpp
)

add_library(Collapsible_Widget STATIC ${COLLAPSIBLE_WIDGET_SOURCES})

set_target_properties(Collapsible_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(Collapsible_Widget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

# Include directories
target_include_directories(Collapsible_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Collapsible_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Collapsible_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(Collapsible_Widget PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(Collapsible_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)