find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGL OpenGLWidgets)

set(EVENT_PLOT_WIDGET_SOURCES

        Core/EventPlotState.cpp
        Core/EventPlotState.hpp

        Rendering/EventPlotOpenGLWidget.cpp
        Rendering/EventPlotOpenGLWidget.hpp

        EventPlotWidgetRegistration.cpp
        EventPlotWidgetRegistration.hpp

        UI/EventPlotAxisWidget.cpp
        UI/EventPlotAxisWidget.hpp
        UI/EventPlotWidget.cpp
        UI/EventPlotWidget.hpp
        UI/EventPlotWidget.ui
        UI/EventPlotPropertiesWidget.cpp
        UI/EventPlotPropertiesWidget.hpp
        UI/EventPlotPropertiesWidget.ui

)

add_library(EventPlotWidget STATIC ${EVENT_PLOT_WIDGET_SOURCES})

set_target_properties(EventPlotWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(EventPlotWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(EventPlotWidget PRIVATE DataManager)
target_link_libraries(EventPlotWidget PRIVATE EditorState)
target_link_libraries(EventPlotWidget PRIVATE PlotAlignmentWidget)
target_link_libraries(EventPlotWidget PRIVATE CorePlotting)
target_link_libraries(EventPlotWidget PRIVATE NEURALYZER_GEOMETRY)
target_link_libraries(EventPlotWidget PRIVATE PlottingOpenGL)
target_link_libraries(EventPlotWidget PRIVATE glm::glm)

target_include_directories(EventPlotWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(EventPlotWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(EventPlotWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(EventPlotWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(EventPlotWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
