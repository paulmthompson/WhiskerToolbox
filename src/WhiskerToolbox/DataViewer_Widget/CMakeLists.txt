
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGLWidgets OpenGL)

find_package(glm CONFIG REQUIRED)

find_package(Boost REQUIRED)

set(DATAVIEWER_WIDGET_SOURCES

        Core/DataViewerStateData.hpp
        Core/DataViewerState.cpp
        Core/DataViewerState.hpp
        Core/SeriesOptionsRegistry.cpp
        Core/SeriesOptionsRegistry.hpp
        Core/SpikeSorterConfigLoader.cpp
        Core/SpikeSorterConfigLoader.hpp
        Core/TimeSeriesDataStore.cpp
        Core/TimeSeriesDataStore.hpp

        DataViewerWidgetRegistration.cpp
        DataViewerWidgetRegistration.hpp

        Interaction/DataViewerCoordinates.cpp
        Interaction/DataViewerCoordinates.hpp
        Interaction/DataViewerInputHandler.cpp
        Interaction/DataViewerInputHandler.hpp
        Interaction/DataViewerInteractionManager.cpp
        Interaction/DataViewerInteractionManager.hpp
        Interaction/DataViewerSelectionManager.cpp
        Interaction/DataViewerSelectionManager.hpp
        Interaction/DataViewerTooltipController.cpp
        Interaction/DataViewerTooltipController.hpp

        Rendering/AnalogVertexCache.cpp
        Rendering/AnalogVertexCache.hpp
        Rendering/OpenGLWidget.cpp
        Rendering/OpenGLWidget.hpp
        Rendering/SceneBuildingHelpers.cpp
        Rendering/SceneBuildingHelpers.hpp
        Rendering/SVGExporter.cpp
        Rendering/SVGExporter.hpp

        UI/DataViewer_Widget.cpp
        UI/DataViewer_Widget.hpp
        UI/DataViewer_Widget.ui
        UI/DataViewerPropertiesWidget.cpp
        UI/DataViewerPropertiesWidget.hpp
        UI/DataViewerPropertiesWidget.ui
        UI/Feature_Tree_Model.hpp
        UI/Feature_Tree_Model.cpp
        UI/SubWidgets/AnalogTimeSeries/AnalogViewer_Widget.cpp
        UI/SubWidgets/AnalogTimeSeries/AnalogViewer_Widget.hpp
        UI/SubWidgets/AnalogTimeSeries/AnalogViewer_Widget.ui
        UI/SubWidgets/DigitalEvent/EventViewer_Widget.cpp
        UI/SubWidgets/DigitalEvent/EventViewer_Widget.hpp
        UI/SubWidgets/DigitalEvent/EventViewer_Widget.ui
        UI/SubWidgets/DigitalInterval/IntervalViewer_Widget.cpp
        UI/SubWidgets/DigitalInterval/IntervalViewer_Widget.hpp
        UI/SubWidgets/DigitalInterval/IntervalViewer_Widget.ui

)

add_library(DataViewer_Widget STATIC ${DATAVIEWER_WIDGET_SOURCES})

set_target_properties(DataViewer_Widget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(DataViewer_Widget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(DataViewer_Widget PRIVATE DataManager)
target_link_libraries(DataViewer_Widget PRIVATE glm::glm)
target_link_libraries(DataViewer_Widget PRIVATE Boost::boost)
target_link_libraries(DataViewer_Widget PRIVATE DataViewer)
target_link_libraries(DataViewer_Widget PRIVATE Feature_Tree_Widget)
target_link_libraries(DataViewer_Widget PRIVATE PlottingOpenGL)  # Contains ShaderManager
target_link_libraries(DataViewer_Widget PRIVATE ColorPicker_Widget)
target_link_libraries(DataViewer_Widget PRIVATE TimeScrollBar)
target_link_libraries(DataViewer_Widget PRIVATE CorePlotting)
target_link_libraries(DataViewer_Widget PRIVATE EditorState)

target_include_directories(DataViewer_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)


# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataViewer_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataViewer_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataViewer_Widget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(DataViewer_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
) 