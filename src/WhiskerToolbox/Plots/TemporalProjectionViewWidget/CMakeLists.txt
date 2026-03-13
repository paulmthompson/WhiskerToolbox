find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGL OpenGLWidgets)

set(TEMPORAL_PROJECTION_VIEW_WIDGET_SOURCES

        Core/TemporalProjectionViewState.cpp
        Core/TemporalProjectionViewState.hpp

        Rendering/TemporalProjectionOpenGLWidget.cpp
        Rendering/TemporalProjectionOpenGLWidget.hpp

        TemporalProjectionViewWidgetRegistration.cpp
        TemporalProjectionViewWidgetRegistration.hpp

        UI/TemporalProjectionViewWidget.cpp
        UI/TemporalProjectionViewWidget.hpp
        UI/TemporalProjectionViewWidget.ui
        UI/TemporalProjectionViewPropertiesWidget.cpp
        UI/TemporalProjectionViewPropertiesWidget.hpp
        UI/TemporalProjectionViewPropertiesWidget.ui

)

add_library(TemporalProjectionViewWidget STATIC ${TEMPORAL_PROJECTION_VIEW_WIDGET_SOURCES})

set_target_properties(TemporalProjectionViewWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(TemporalProjectionViewWidget PUBLIC 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(TemporalProjectionViewWidget PUBLIC EditorState)
target_link_libraries(TemporalProjectionViewWidget PUBLIC DataTypeEnum)

target_link_libraries(TemporalProjectionViewWidget PRIVATE DataManager)
target_link_libraries(TemporalProjectionViewWidget PRIVATE WhiskerToolbox::LineData)
target_link_libraries(TemporalProjectionViewWidget PRIVATE WhiskerToolbox::PointData)

target_link_libraries(TemporalProjectionViewWidget PRIVATE Collapsible_Widget)
target_link_libraries(TemporalProjectionViewWidget PRIVATE CorePlotting)
target_link_libraries(TemporalProjectionViewWidget PRIVATE PlottingOpenGL)
target_link_libraries(TemporalProjectionViewWidget PRIVATE HorizontalAxisWidget)
target_link_libraries(TemporalProjectionViewWidget PRIVATE VerticalAxisWidget)
target_link_libraries(TemporalProjectionViewWidget PRIVATE GlyphStyleWidget)
target_link_libraries(TemporalProjectionViewWidget PRIVATE GroupContextMenu)

target_include_directories(TemporalProjectionViewWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(TemporalProjectionViewWidget)

# Enable automoc for Qt
set_target_properties(TemporalProjectionViewWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
enable_whiskertoolbox_shared_pch(TemporalProjectionViewWidget)