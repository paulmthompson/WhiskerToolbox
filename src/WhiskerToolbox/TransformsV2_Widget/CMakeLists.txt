
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(TRANSFORMS_V2_WIDGET_SOURCES
        Core/TransformsV2State.hpp
        Core/TransformsV2State.cpp
        UI/ParamWidgetRegistry.hpp
        UI/PipelineStepListWidget.hpp
        UI/PipelineStepListWidget.cpp
        UI/StepConfigPanel.hpp
        UI/StepConfigPanel.cpp
        UI/TransformsV2Properties_Widget.hpp
        UI/TransformsV2Properties_Widget.cpp
        UI/TransformsV2Properties_Widget.ui
        TransformsV2WidgetRegistration.hpp
        TransformsV2WidgetRegistration.cpp
)

qt_add_library(TransformsV2_Widget STATIC ${TRANSFORMS_V2_WIDGET_SOURCES})

target_include_directories(TransformsV2_Widget PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)

target_link_libraries(TransformsV2_Widget PUBLIC
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
        EditorState
        DataManager
        TransformsV2
        AutoParamWidget
        Collapsible_Widget
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(TransformsV2_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(TransformsV2_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(TransformsV2_Widget PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_pch(TransformsV2_Widget)