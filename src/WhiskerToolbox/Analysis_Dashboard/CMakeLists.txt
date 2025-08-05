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
    DataSourceRegistry/DataSourceRegistry.hpp
    DataSourceRegistry/DataSourceRegistry.cpp

    # Plots
    Plots/AbstractPlotWidget.hpp
    Plots/AbstractPlotWidget.cpp
    Plots/AbstractPlotParameters.hpp
    Plots/AbstractPlotParameters.cpp

    PlotOrganizers/AbstractPlotOrganizer.hpp
    PlotOrganizers/GraphicsScenePlotOrganizer.hpp
    PlotOrganizers/GraphicsScenePlotOrganizer.cpp

    # Groups
    Groups/GroupManager.hpp
    Groups/GroupManager.cpp
    Groups/GroupCoordinator.cpp
    Groups/GroupCoordinator.hpp
    Groups/GroupManagementWidget.hpp
    Groups/GroupManagementWidget.cpp
    Groups/GroupManagementWidget.ui

    # Toolbox
    Toolbox/ToolboxPanel.hpp
    Toolbox/ToolboxPanel.cpp
    Toolbox/ToolboxPanel.ui

    # Tables
    Tables/TableManager.hpp
    Tables/TableManager.cpp
    Tables/TableDesignerWidget.hpp
    Tables/TableDesignerWidget.cpp
    Tables/TableDesignerWidget.ui

    # Widgets
    Widgets/ScatterPlotWidget/ScatterPlotWidget.hpp
    Widgets/ScatterPlotWidget/ScatterPlotWidget.cpp
    Widgets/ScatterPlotWidget/ScatterPlotOpenGLWidget.hpp
    Widgets/ScatterPlotWidget/ScatterPlotOpenGLWidget.cpp
    Widgets/ScatterPlotWidget/ScatterPlotPropertiesWidget.hpp
    Widgets/ScatterPlotWidget/ScatterPlotPropertiesWidget.cpp
    Widgets/ScatterPlotWidget/ScatterPlotPropertiesWidget.ui
    Widgets/ScatterPlotWidget/ScatterPlotVisualization.hpp
    Widgets/ScatterPlotWidget/ScatterPlotVisualization.cpp

    Widgets/EventPlotWidget/EventPlotWidget.hpp
    Widgets/EventPlotWidget/EventPlotWidget.cpp
    Widgets/EventPlotWidget/EventPlotPropertiesWidget.hpp
    Widgets/EventPlotWidget/EventPlotPropertiesWidget.cpp
    Widgets/EventPlotWidget/EventPlotPropertiesWidget.ui
    Widgets/EventPlotWidget/EventPlotOpenGLWidget.hpp
    Widgets/EventPlotWidget/EventPlotOpenGLWidget.cpp
    Widgets/EventPlotWidget/EventPointVisualization.hpp
    Widgets/EventPlotWidget/EventPointVisualization.cpp

    Widgets/SpatialOverlayPlotWidget/Lines/LineDataVisualization.hpp
    Widgets/SpatialOverlayPlotWidget/Lines/LineDataVisualization.cpp
    Widgets/SpatialOverlayPlotWidget/Masks/MaskDataVisualization.hpp
    Widgets/SpatialOverlayPlotWidget/Masks/MaskDataVisualization.cpp
    Widgets/SpatialOverlayPlotWidget/Points/PointDataVisualization.hpp
    Widgets/SpatialOverlayPlotWidget/Points/PointDataVisualization.cpp
    Widgets/SpatialOverlayPlotWidget/Points/GenericPointVisualization.hpp
    Widgets/SpatialOverlayPlotWidget/Points/VectorPointVisualization.hpp
    Widgets/SpatialOverlayPlotWidget/Selection/LineSelectionHandler.hpp
    Widgets/SpatialOverlayPlotWidget/Selection/LineSelectionHandler.cpp
    Widgets/SpatialOverlayPlotWidget/Selection/NoneSelectionHandler.hpp
    Widgets/SpatialOverlayPlotWidget/Selection/PointSelectionHandler.hpp
    Widgets/SpatialOverlayPlotWidget/Selection/PointSelectionHandler.cpp
    Widgets/SpatialOverlayPlotWidget/Selection/PolygonSelectionHandler.hpp
    Widgets/SpatialOverlayPlotWidget/Selection/PolygonSelectionHandler.cpp
    Widgets/SpatialOverlayPlotWidget/Selection/SelectionHandlers.hpp
    Widgets/SpatialOverlayPlotWidget/Selection/SelectionModes.hpp
    Widgets/SpatialOverlayPlotWidget/Selection/SelectionModes.cpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayOpenGLWidget.hpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayOpenGLWidget.cpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.cpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.hpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.cpp
    Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.ui
    Widgets/SpatialOverlayPlotWidget/RenderingContext.hpp

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

target_link_libraries(Analysis_Dashboard PRIVATE TimeScrollBar)
target_link_libraries(Analysis_Dashboard PRIVATE DataManager)
target_link_libraries(Analysis_Dashboard PRIVATE NEURALYZER_GEOMETRY)
target_link_libraries(Analysis_Dashboard PRIVATE ShaderManager)
target_link_libraries(Analysis_Dashboard PRIVATE Feature_Table_Widget)

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

