
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
        EditorState)

 # Type Chain Result must be made into pointer
 # Then can be made private
target_link_libraries(TransformsV2_Widget PUBLIC TransformsV2)

target_link_libraries(TransformsV2_Widget PRIVATE AutoParamWidget)
target_link_libraries(TransformsV2_Widget PRIVATE Collapsible_Widget)
target_link_libraries(TransformsV2_Widget PRIVATE DataManager)

set_target_compiler_warnings(TransformsV2_Widget)

enable_whiskertoolbox_shared_pch(TransformsV2_Widget)