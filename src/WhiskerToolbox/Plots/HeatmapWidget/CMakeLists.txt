find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(HEATMAP_WIDGET_SOURCES

        Core/HeatmapState.cpp
        Core/HeatmapState.hpp

        HeatmapWidgetRegistration.cpp
        HeatmapWidgetRegistration.hpp

        UI/HeatmapWidget.cpp
        UI/HeatmapWidget.hpp
        UI/HeatmapWidget.ui
        UI/HeatmapPropertiesWidget.cpp
        UI/HeatmapPropertiesWidget.hpp
        UI/HeatmapPropertiesWidget.ui

)

add_library(HeatmapWidget STATIC ${HEATMAP_WIDGET_SOURCES})

set_target_properties(HeatmapWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(HeatmapWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(HeatmapWidget PRIVATE DataManager)
target_link_libraries(HeatmapWidget PRIVATE EditorState)
target_link_libraries(HeatmapWidget PRIVATE PlotAlignmentWidget)

target_include_directories(HeatmapWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(HeatmapWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(HeatmapWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(HeatmapWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(HeatmapWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
