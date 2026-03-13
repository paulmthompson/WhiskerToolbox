find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGL OpenGLWidgets)

set(THREE_D_PLOT_WIDGET_SOURCES

        Core/3DPlotState.cpp
        Core/3DPlotState.hpp

        Rendering/ThreeDPlotOpenGLWidget.cpp
        Rendering/ThreeDPlotOpenGLWidget.hpp

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

target_link_libraries(3DPlotWidget PUBLIC 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(3DPlotWidget PUBLIC DataTypeEnum)
target_link_libraries(3DPlotWidget PUBLIC EditorState)

target_link_libraries(3DPlotWidget PRIVATE DataManager)
target_link_libraries(3DPlotWidget PRIVATE WhiskerToolbox::PointData)

target_include_directories(3DPlotWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(3DPlotWidget)

# Enable automoc for Qt
set_target_properties(3DPlotWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(3DPlotWidget)