find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(reflectcpp CONFIG REQUIRED)

add_library(DataSynthesizer_Widget STATIC
    DataSynthesizerWidgetRegistration.hpp
    DataSynthesizerWidgetRegistration.cpp
    Core/DataSynthesizerStateData.hpp
    Core/DataSynthesizerState.hpp
    Core/DataSynthesizerState.cpp
    UI/DataSynthesizerProperties_Widget.hpp
    UI/DataSynthesizerProperties_Widget.cpp
    UI/DataSynthesizerView_Widget.hpp
    UI/DataSynthesizerView_Widget.cpp
)

target_link_libraries(DataSynthesizer_Widget
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        EditorState
        DataManager
        reflectcpp::reflectcpp
)

target_include_directories(DataSynthesizer_Widget PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_properties(DataSynthesizer_Widget PROPERTIES
    AUTOMOC ON
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataSynthesizer_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataSynthesizer_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataSynthesizer_Widget PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_pch(DataSynthesizer_Widget)
