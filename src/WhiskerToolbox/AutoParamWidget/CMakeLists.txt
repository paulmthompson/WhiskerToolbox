find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(AUTO_PARAM_WIDGET_SOURCES
        AutoParamWidget.hpp
        AutoParamWidget.cpp
)

qt_add_library(AutoParamWidget STATIC ${AUTO_PARAM_WIDGET_SOURCES})

target_include_directories(AutoParamWidget PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
)

target_link_libraries(AutoParamWidget PUBLIC
        Qt6::Widgets
        WhiskerToolbox::ParameterSchema
)

set_target_compiler_warnings(AutoParamWidget)

enable_whiskertoolbox_shared_pch(AutoParamWidget)