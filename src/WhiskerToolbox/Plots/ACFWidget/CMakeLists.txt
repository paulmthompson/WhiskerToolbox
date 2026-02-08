find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGL OpenGLWidgets)

set(ACF_WIDGET_SOURCES

        Core/ACFState.cpp
        Core/ACFState.hpp

        Rendering/ACFOpenGLWidget.cpp
        Rendering/ACFOpenGLWidget.hpp

        ACFWidgetRegistration.cpp
        ACFWidgetRegistration.hpp

        UI/ACFWidget.cpp
        UI/ACFWidget.hpp
        UI/ACFWidget.ui
        UI/ACFPropertiesWidget.cpp
        UI/ACFPropertiesWidget.hpp
        UI/ACFPropertiesWidget.ui

)

add_library(ACFWidget STATIC ${ACF_WIDGET_SOURCES})

set_target_properties(ACFWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(ACFWidget PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(ACFWidget PRIVATE DataManager)
target_link_libraries(ACFWidget PRIVATE EditorState)
target_link_libraries(ACFWidget PRIVATE HorizontalAxisWidget)
target_link_libraries(ACFWidget PRIVATE VerticalAxisWidget)
target_link_libraries(ACFWidget PRIVATE Collapsible_Widget)
target_link_libraries(ACFWidget PRIVATE CorePlotting)
target_link_libraries(ACFWidget PRIVATE PlottingOpenGL)
target_link_libraries(ACFWidget PRIVATE glm::glm)

target_include_directories(ACFWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ACFWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ACFWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(ACFWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(ACFWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
