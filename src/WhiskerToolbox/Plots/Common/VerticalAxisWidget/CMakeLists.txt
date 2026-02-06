find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(VERTICAL_AXIS_WIDGET_SOURCES

        Core/VerticalAxisState.cpp
        Core/VerticalAxisState.hpp
        Core/VerticalAxisStateData.hpp
        VerticalAxisWidget.cpp
        VerticalAxisWidget.hpp
        VerticalAxisWithRangeControls.cpp
        VerticalAxisWithRangeControls.hpp

)

add_library(VerticalAxisWidget STATIC ${VERTICAL_AXIS_WIDGET_SOURCES})

set_target_properties(VerticalAxisWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(VerticalAxisWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_include_directories(VerticalAxisWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(VerticalAxisWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(VerticalAxisWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(VerticalAxisWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(VerticalAxisWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
