find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGL OpenGLWidgets)

set(PSTH_WIDGET_SOURCES

        Core/PSTHState.cpp
        Core/PSTHState.hpp
        Core/ViewStateAdapter.cpp
        Core/ViewStateAdapter.hpp

        Rendering/PSTHPlotOpenGLWidget.cpp
        Rendering/PSTHPlotOpenGLWidget.hpp

        PSTHWidgetRegistration.cpp
        PSTHWidgetRegistration.hpp

        UI/PSTHWidget.cpp
        UI/PSTHWidget.hpp
        UI/PSTHWidget.ui
        UI/PSTHPropertiesWidget.cpp
        UI/PSTHPropertiesWidget.hpp
        UI/PSTHPropertiesWidget.ui

)

add_library(PSTHWidget STATIC ${PSTH_WIDGET_SOURCES})

set_target_properties(PSTHWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(PSTHWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(PSTHWidget PRIVATE DataManager)
target_link_libraries(PSTHWidget PRIVATE EditorState)
target_link_libraries(PSTHWidget PRIVATE PlotAlignmentWidget)
target_link_libraries(PSTHWidget PRIVATE RelativeTimeAxisWidget)
target_link_libraries(PSTHWidget PRIVATE VerticalAxisWidget)
target_link_libraries(PSTHWidget PRIVATE CorePlotting)
target_link_libraries(PSTHWidget PRIVATE NEURALYZER_GEOMETRY)
target_link_libraries(PSTHWidget PRIVATE PlottingOpenGL)
target_link_libraries(PSTHWidget PRIVATE glm::glm)

target_include_directories(PSTHWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(PSTHWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(PSTHWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(PSTHWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(PSTHWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
