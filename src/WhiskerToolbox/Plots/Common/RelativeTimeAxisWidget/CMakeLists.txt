find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(RELATIVE_TIME_AXIS_WIDGET_SOURCES

        RelativeTimeAxisWidget.cpp
        RelativeTimeAxisWidget.hpp
        RelativeTimeAxisWithRangeControls.cpp
        RelativeTimeAxisWithRangeControls.hpp

)

add_library(RelativeTimeAxisWidget STATIC ${RELATIVE_TIME_AXIS_WIDGET_SOURCES})

set_target_properties(RelativeTimeAxisWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(RelativeTimeAxisWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(RelativeTimeAxisWidget PRIVATE CorePlotting)

target_include_directories(RelativeTimeAxisWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(RelativeTimeAxisWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(RelativeTimeAxisWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(RelativeTimeAxisWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(RelativeTimeAxisWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
