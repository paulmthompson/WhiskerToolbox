find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui OpenGL OpenGLWidgets)

set(ONION_SKIN_VIEW_WIDGET_SOURCES

        Core/OnionSkinViewState.cpp
        Core/OnionSkinViewState.hpp

        Rendering/OnionSkinViewOpenGLWidget.cpp
        Rendering/OnionSkinViewOpenGLWidget.hpp

        OnionSkinViewWidgetRegistration.cpp
        OnionSkinViewWidgetRegistration.hpp

        UI/OnionSkinViewWidget.cpp
        UI/OnionSkinViewWidget.hpp
        UI/OnionSkinViewWidget.ui
        UI/OnionSkinViewPropertiesWidget.cpp
        UI/OnionSkinViewPropertiesWidget.hpp
        UI/OnionSkinViewPropertiesWidget.ui

)

add_library(OnionSkinViewWidget STATIC ${ONION_SKIN_VIEW_WIDGET_SOURCES})

set_target_properties(OnionSkinViewWidget PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(OnionSkinViewWidget PRIVATE 
    Qt6::Widgets 
    Qt6::Core 
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_link_libraries(OnionSkinViewWidget PRIVATE Collapsible_Widget)
target_link_libraries(OnionSkinViewWidget PRIVATE CorePlotting)
target_link_libraries(OnionSkinViewWidget PRIVATE DataManager)
target_link_libraries(OnionSkinViewWidget PRIVATE EditorState)
target_link_libraries(OnionSkinViewWidget PRIVATE HorizontalAxisWidget)
target_link_libraries(OnionSkinViewWidget PRIVATE VerticalAxisWidget)

target_include_directories(OnionSkinViewWidget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" #This directory
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>" #For data manager 
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Compiler-specific options (following the same pattern as main project)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(OnionSkinViewWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(OnionSkinViewWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(OnionSkinViewWidget PRIVATE ${MSVC_WARNINGS})
endif()

# Enable automoc for Qt
set_target_properties(OnionSkinViewWidget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
