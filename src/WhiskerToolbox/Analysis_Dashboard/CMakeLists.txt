# Analysis_Dashboard Library CMakeLists.txt

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Xml Svg PrintSupport OpenGLWidgets OpenGL)

# Define the Analysis_Dashboard library sources
set(ANALYSIS_DASHBOARD_SOURCES
    # Core files
    Analysis_Dashboard.hpp
    Analysis_Dashboard.cpp
    Analysis_Dashboard.ui

    # Abstract classes and interfaces
    PlotContainer.hpp 
    PlotContainer.cpp
    PlotFactory.hpp 
    PlotFactory.cpp

    # Common 
    Widgets/Common/PlotInteractionController.hpp
    Widgets/Common/PlotInteractionController.cpp
    Widgets/Common/ViewAdapter.hpp
    Widgets/Common/GenericViewAdapter.hpp
    Widgets/Common/GenericViewAdapter.cpp
    Widgets/Common/BasePlotOpenGLWidget.hpp
    Widgets/Common/BasePlotOpenGLWidget.cpp
    Widgets/Common/TooltipManager.hpp
    Widgets/Common/TooltipManager.cpp
    Widgets/Common/ViewState.hpp
    Widgets/Common/ViewState.cpp


    # Plots
    Plots/AbstractPlotWidget.hpp
    Plots/AbstractPlotWidget.cpp
    Plots/AbstractPlotParameters.hpp
    Plots/AbstractPlotParameters.cpp

    PlotOrganizers/AbstractPlotOrganizer.hpp
    PlotOrganizers/GraphicsScenePlotOrganizer.hpp
    PlotOrganizers/GraphicsScenePlotOrganizer.cpp
    PlotOrganizers/DockingPlotOrganizer.hpp
    PlotOrganizers/DockingPlotOrganizer.cpp
    PlotOrganizers/PlotDockWidget.hpp
    PlotOrganizers/PlotDockWidget.cpp
    PlotOrganizers/PlotDockWidgetContent.hpp
    PlotOrganizers/PlotDockWidgetContent.cpp

    # Groups
    Groups/GroupCoordinator.cpp
    Groups/GroupCoordinator.hpp

    Selection/LineSelectionHandler.hpp
    Selection/LineSelectionHandler.cpp
    Selection/NoneSelectionHandler.hpp
    Selection/PointSelectionHandler.hpp
    Selection/PointSelectionHandler.cpp
    Selection/PolygonSelectionHandler.hpp
    Selection/PolygonSelectionHandler.cpp
    Selection/SelectionHandlers.hpp
    Selection/SelectionModes.hpp
    Selection/SelectionModes.cpp

    # Toolbox
    Toolbox/ToolboxPanel.hpp
    Toolbox/ToolboxPanel.cpp
    Toolbox/ToolboxPanel.ui

    # Widgets
    Widgets/ScatterPlotWidget/ScatterPlotWidget.hpp
    Widgets/ScatterPlotWidget/ScatterPlotWidget.cpp
   # Widgets/ScatterPlotWidget/ScatterPlotOpenGLWidget.hpp
   # Widgets/ScatterPlotWidget/ScatterPlotOpenGLWidget.cpp
    Widgets/ScatterPlotWidget/ScatterPlotOpenGLWidget_Refactored.hpp
    Widgets/ScatterPlotWidget/ScatterPlotOpenGLWidget_Refactored.cpp
    Widgets/ScatterPlotWidget/ScatterPlotPropertiesWidget.hpp
    Widgets/ScatterPlotWidget/ScatterPlotPropertiesWidget.cpp
    Widgets/ScatterPlotWidget/ScatterPlotPropertiesWidget.ui

    Widgets/EventPlotWidget/EventPlotWidget.hpp
    Widgets/EventPlotWidget/EventPlotWidget.cpp
    Widgets/EventPlotWidget/EventPlotPropertiesWidget.hpp
    Widgets/EventPlotWidget/EventPlotPropertiesWidget.cpp
    Widgets/EventPlotWidget/EventPlotPropertiesWidget.ui
    Widgets/EventPlotWidget/EventPlotOpenGLWidget.hpp
    Widgets/EventPlotWidget/EventPlotOpenGLWidget.cpp
    Widgets/EventPlotWidget/EventPointVisualization.hpp
    Widgets/EventPlotWidget/EventPointVisualization.cpp
    Widgets/EventPlotWidget/EventPlotViewAdapter.hpp
    Widgets/EventPlotWidget/EventPlotViewAdapter.cpp

    Widgets/SpatialOverlayPlotWidget/SpatialOverlayOpenGLWidget.hpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayOpenGLWidget.cpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.cpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.hpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.cpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.ui

    Visualizers/RenderingContext.hpp
    Visualizers/Lines/LineDataVisualization.hpp
    Visualizers/Lines/LineDataVisualization.cpp
    Visualizers/Masks/MaskDataVisualization.hpp
    Visualizers/Masks/MaskDataVisualization.cpp
    Visualizers/Points/PointDataVisualization.hpp
    Visualizers/Points/PointDataVisualization.cpp
    Visualizers/Points/GenericPointVisualization.hpp
    Visualizers/Points/VectorPointVisualization.hpp
    Visualizers/Points/ScatterPlotVisualization.hpp
    Visualizers/Points/ScatterPlotVisualization.cpp

    # Properties
    Properties/PropertiesPanel.hpp
    Properties/PropertiesPanel.cpp
    Properties/PropertiesPanel.ui
    Properties/AbstractPlotPropertiesWidget.hpp
    Properties/AbstractPlotPropertiesWidget.cpp
    Properties/GlobalPropertiesWidget.hpp
    Properties/GlobalPropertiesWidget.cpp
    Properties/GlobalPropertiesWidget.ui
)

# Create the Analysis_Dashboard library
add_library(Analysis_Dashboard STATIC ${ANALYSIS_DASHBOARD_SOURCES})

# Set target properties
set_target_properties(Analysis_Dashboard PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Link Qt libraries
target_link_libraries(Analysis_Dashboard PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Xml 
    Qt6::Svg 
    Qt6::PrintSupport 
    Qt6::OpenGLWidgets 
    Qt6::OpenGL
)
target_link_libraries(Analysis_Dashboard PRIVATE qt6advanceddocking)

target_link_libraries(Analysis_Dashboard PRIVATE TimeScrollBar)
target_link_libraries(Analysis_Dashboard PRIVATE DataManager)
target_link_libraries(Analysis_Dashboard PRIVATE NEURALYZER_GEOMETRY)
target_link_libraries(Analysis_Dashboard PRIVATE ShaderManager)
target_link_libraries(Analysis_Dashboard PRIVATE Feature_Table_Widget)
target_link_libraries(Analysis_Dashboard PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(Analysis_Dashboard PRIVATE GroupManagementWidget)

# Include directories
target_include_directories(Analysis_Dashboard PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../TimeScrollBar>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compile definitions
target_compile_definitions(Analysis_Dashboard PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Analysis_Dashboard PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Analysis_Dashboard PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(Analysis_Dashboard PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(Analysis_Dashboard PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

