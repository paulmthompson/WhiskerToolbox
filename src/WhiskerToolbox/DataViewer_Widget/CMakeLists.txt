
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGLWidgets OpenGL)

find_package(glm CONFIG REQUIRED)

find_package(Boost REQUIRED)

set(DATAVIEWER_WIDGET_SOURCES
    AnalogVertexCache.cpp
    AnalogVertexCache.hpp
    DataViewer_Widget.cpp
        DataViewer_Widget.hpp
        DataViewer_Widget.ui
        DataViewerCoordinates.cpp
        DataViewerCoordinates.hpp
        DataViewerInputHandler.cpp
        DataViewerInputHandler.hpp
        DataViewerInteractionManager.cpp
        DataViewerInteractionManager.hpp
        DataViewerSelectionManager.cpp
        DataViewerSelectionManager.hpp
        DataViewerStateData.hpp
        DataViewerTooltipController.cpp
        DataViewerTooltipController.hpp
        SeriesOptionsRegistry.cpp
        SeriesOptionsRegistry.hpp
        OpenGLWidget.cpp
        OpenGLWidget.hpp
        SceneBuildingHelpers.cpp
        SceneBuildingHelpers.hpp
        SpikeSorterConfigLoader.cpp
        SpikeSorterConfigLoader.hpp
        SVGExporter.cpp
        SVGExporter.hpp
        TimeSeriesDataStore.cpp
        TimeSeriesDataStore.hpp
        AnalogTimeSeries/AnalogViewer_Widget.cpp
        AnalogTimeSeries/AnalogViewer_Widget.hpp
        AnalogTimeSeries/AnalogViewer_Widget.ui
        DigitalEvent/EventViewer_Widget.cpp
        DigitalEvent/EventViewer_Widget.hpp
        DigitalEvent/EventViewer_Widget.ui
        DigitalInterval/IntervalViewer_Widget.cpp
        DigitalInterval/IntervalViewer_Widget.hpp
        DigitalInterval/IntervalViewer_Widget.ui
        Feature_Tree_Model.hpp
        Feature_Tree_Model.cpp
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