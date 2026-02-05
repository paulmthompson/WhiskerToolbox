find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGL OpenGLWidgets)

set(SCATTER_PLOT_WIDGET_SOURCES

        Core/ScatterPlotState.cpp
        Core/ScatterPlotState.hpp

        Rendering/ScatterPlotOpenGLWidget.cpp
        Rendering/ScatterPlotOpenGLWidget.hpp

        ScatterPlotWidgetRegistration.cpp
        ScatterPlotWidgetRegistration.hpp

        UI/ScatterPlotWidget.cpp
        UI/ScatterPlotWidget.hpp
        UI/ScatterPlotWidget.ui
        UI/ScatterPlotPropertiesWidget.cpp
        UI/ScatterPlotPropertiesWidget.hpp
        UI/ScatterPlotPropertiesWidget.ui

)

add_library(ScatterPlotWidget STATIC ${SCATTER_PLOT_WIDGET_SOURCES})

set_target_properties(ScatterPlotWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(ScatterPlotWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(ScatterPlotWidget PRIVATE DataManager)
target_link_libraries(ScatterPlotWidget PRIVATE EditorState)
target_link_libraries(ScatterPlotWidget PRIVATE HorizontalAxisWidget)
target_link_libraries(ScatterPlotWidget PRIVATE VerticalAxisWidget)

target_include_directories(ScatterPlotWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ScatterPlotWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ScatterPlotWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(ScatterPlotWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(ScatterPlotWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
