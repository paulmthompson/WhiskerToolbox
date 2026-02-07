find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(HORIZONTAL_AXIS_WIDGET_SOURCES

        Core/HorizontalAxisState.cpp
        Core/HorizontalAxisState.hpp
        Core/HorizontalAxisStateData.hpp
        HorizontalAxisWidget.cpp
        HorizontalAxisWidget.hpp
        HorizontalAxisWithRangeControls.cpp
        HorizontalAxisWithRangeControls.hpp

)

add_library(HorizontalAxisWidget STATIC ${HORIZONTAL_AXIS_WIDGET_SOURCES})

set_target_properties(HorizontalAxisWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(HorizontalAxisWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(HorizontalAxisWidget PUBLIC CorePlotting)

target_include_directories(HorizontalAxisWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(HorizontalAxisWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(HorizontalAxisWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(HorizontalAxisWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(HorizontalAxisWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
