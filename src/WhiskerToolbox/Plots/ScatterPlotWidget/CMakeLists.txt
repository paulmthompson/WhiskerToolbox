find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGL OpenGLWidgets)

set(SCATTER_PLOT_WIDGET_SOURCES

        Core/BuildScatterPoints.cpp
        Core/BuildScatterPoints.hpp
        Core/ScatterAxisSource.hpp
        Core/ScatterColorConfig.hpp
        Core/ScatterPlotState.cpp
        Core/ScatterPlotState.hpp
        Core/ScatterPointData.hpp
        Core/SourceCompatibility.cpp
        Core/SourceCompatibility.hpp

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

target_link_libraries(ScatterPlotWidget PUBLIC 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(ScatterPlotWidget PUBLIC DataTypeEnum)
target_link_libraries(ScatterPlotWidget PUBLIC EditorState)

target_link_libraries(ScatterPlotWidget PRIVATE DataManager)
target_link_libraries(ScatterPlotWidget PRIVATE WhiskerToolbox::AnalogTimeSeries)
target_link_libraries(ScatterPlotWidget PRIVATE WhiskerToolbox::TensorData)
target_link_libraries(ScatterPlotWidget PRIVATE WhiskerToolbox::DigitalTimeSeries)

target_link_libraries(ScatterPlotWidget PRIVATE HorizontalAxisWidget)
target_link_libraries(ScatterPlotWidget PRIVATE VerticalAxisWidget)
target_link_libraries(ScatterPlotWidget PRIVATE Collapsible_Widget)
target_link_libraries(ScatterPlotWidget PRIVATE GlyphStyleWidget)
target_link_libraries(ScatterPlotWidget PRIVATE ColormapControls)
target_link_libraries(ScatterPlotWidget PRIVATE CorePlotting)
target_link_libraries(ScatterPlotWidget PRIVATE PlottingOpenGL)
target_link_libraries(ScatterPlotWidget PRIVATE PlottingSVG)
target_link_libraries(ScatterPlotWidget PRIVATE TooltipManager)
target_link_libraries(ScatterPlotWidget PRIVATE GroupContextMenu)
target_link_libraries(ScatterPlotWidget PRIVATE KeymapSystem)
target_link_libraries(ScatterPlotWidget PRIVATE WhiskerToolbox::StateManagement)


target_include_directories(ScatterPlotWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(ScatterPlotWidget)

# Enable automoc for Qt
set_target_properties(ScatterPlotWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(ScatterPlotWidget)