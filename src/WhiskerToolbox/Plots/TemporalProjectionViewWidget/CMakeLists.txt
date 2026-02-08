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

target_link_libraries(TemporalProjectionViewWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(TemporalProjectionViewWidget PRIVATE Collapsible_Widget)
target_link_libraries(TemporalProjectionViewWidget PRIVATE CorePlotting)
target_link_libraries(TemporalProjectionViewWidget PRIVATE DataManager)
target_link_libraries(TemporalProjectionViewWidget PRIVATE EditorState)
target_link_libraries(TemporalProjectionViewWidget PRIVATE HorizontalAxisWidget)
target_link_libraries(TemporalProjectionViewWidget PRIVATE VerticalAxisWidget)

target_include_directories(TemporalProjectionViewWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(TemporalProjectionViewWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(TemporalProjectionViewWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(TemporalProjectionViewWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(TemporalProjectionViewWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
