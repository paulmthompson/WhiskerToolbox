find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(THREE_D_PLOT_WIDGET_SOURCES

        Core/3DPlotState.cpp
        Core/3DPlotState.hpp

        3DPlotWidgetRegistration.cpp
        3DPlotWidgetRegistration.hpp

        UI/3DPlotWidget.cpp
        UI/3DPlotWidget.hpp
        UI/3DPlotWidget.ui
        UI/3DPlotPropertiesWidget.cpp
        UI/3DPlotPropertiesWidget.hpp
        UI/3DPlotPropertiesWidget.ui

)

add_library(3DPlotWidget STATIC ${THREE_D_PLOT_WIDGET_SOURCES})

set_target_properties(3DPlotWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(3DPlotWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
)

target_link_libraries(3DPlotWidget PRIVATE DataManager)
target_link_libraries(3DPlotWidget PRIVATE EditorState)

target_include_directories(3DPlotWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(3DPlotWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(3DPlotWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(3DPlotWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(3DPlotWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
