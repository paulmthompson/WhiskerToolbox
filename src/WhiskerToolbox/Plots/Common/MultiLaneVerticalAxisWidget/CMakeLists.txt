find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(MULTI_LANE_VERTICAL_AXIS_WIDGET_SOURCES

        Core/MultiLaneVerticalAxisState.cpp
        Core/MultiLaneVerticalAxisState.hpp
        Core/MultiLaneVerticalAxisStateData.hpp
        MultiLaneVerticalAxisWidget.cpp
        MultiLaneVerticalAxisWidget.hpp

)

add_library(MultiLaneVerticalAxisWidget STATIC ${MULTI_LANE_VERTICAL_AXIS_WIDGET_SOURCES})

set_target_properties(MultiLaneVerticalAxisWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(MultiLaneVerticalAxisWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_include_directories(MultiLaneVerticalAxisWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For Plots/Common/
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MultiLaneVerticalAxisWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(MultiLaneVerticalAxisWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(MultiLaneVerticalAxisWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(MultiLaneVerticalAxisWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(MultiLaneVerticalAxisWidget)
