# MLCore_Widget Static Library
# Qt6 widget for interactive ML workflows (classification, clustering)
# Uses MLCore for algorithms, DataManager for data access, EditorState for state management

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(reflectcpp CONFIG REQUIRED)

set(MLCORE_WIDGET_SOURCES
    MLCoreWidget.hpp
    MLCoreWidget.cpp
    MLCoreWidgetState.hpp
    MLCoreWidgetState.cpp
    MLCoreWidgetStateData.hpp
    MLCoreWidgetRegistration.hpp
    MLCoreWidgetRegistration.cpp
    FeatureSelectionPanel.hpp
    FeatureSelectionPanel.cpp
    FeatureSelectionPanel.ui
    RegionSelectionPanel.hpp
    RegionSelectionPanel.cpp
    RegionSelectionPanel.ui
    LabelConfigPanel.hpp
    LabelConfigPanel.cpp
    LabelConfigPanel.ui
    ModelConfigPanel.hpp
    ModelConfigPanel.cpp
    ModelConfigPanel.ui
    PredictionPanel.hpp
    PredictionPanel.cpp
    PredictionPanel.ui
    ResultsPanel.hpp
    ResultsPanel.cpp
    ResultsPanel.ui
)

add_library(MLCore_Widget STATIC ${MLCORE_WIDGET_SOURCES})

target_include_directories(MLCore_Widget
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(MLCore_Widget
    PUBLIC
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
        EditorState
    PRIVATE
        DataManager
        MLCore
        reflectcpp::reflectcpp
)

set_target_properties(MLCore_Widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MLCore_Widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(MLCore_Widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(MLCore_Widget PRIVATE ${MSVC_WARNINGS})
endif()

add_library(WhiskerToolbox::MLCore_Widget ALIAS MLCore_Widget)
