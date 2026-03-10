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

# Apply the same compiler flags as the main project
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(AutoParamWidget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(AutoParamWidget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(AutoParamWidget PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_pch(AutoParamWidget)