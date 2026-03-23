# MLCore_Widget Static Library
# Qt6 widget for interactive ML workflows (classification, clustering)
# Uses MLCore for algorithms, DataManager for data access, EditorState for state management

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(reflectcpp CONFIG REQUIRED)

set(MLCORE_WIDGET_SOURCES

    Core/MLCoreWidgetState.hpp
    Core/MLCoreWidgetState.cpp
    Core/MLCoreWidgetStateData.hpp


    MLCoreWidget.hpp
    MLCoreWidget.cpp
    
    MLCoreWidgetRegistration.hpp
    MLCoreWidgetRegistration.cpp
    UI/FeatureSelectionPanel/FeatureSelectionPanel.hpp
    UI/FeatureSelectionPanel/FeatureSelectionPanel.cpp
    UI/FeatureSelectionPanel/FeatureSelectionPanel.ui
    UI/RegionSelectionPanel/RegionSelectionPanel.hpp
    UI/RegionSelectionPanel/RegionSelectionPanel.cpp
    UI/RegionSelectionPanel/RegionSelectionPanel.ui
    UI/LabelConfigPanel/LabelConfigPanel.hpp
    UI/LabelConfigPanel/LabelConfigPanel.cpp
    UI/LabelConfigPanel/LabelConfigPanel.ui
    UI/ModelConfigPanel/ModelConfigPanel.hpp
    UI/ModelConfigPanel/ModelConfigPanel.cpp
    UI/ModelConfigPanel/ModelConfigPanel.ui
    UI/PredictionPanel/PredictionPanel.hpp
    UI/PredictionPanel/PredictionPanel.cpp
    UI/PredictionPanel/PredictionPanel.ui
    UI/ResultsPanel/ResultsPanel.hpp
    UI/ResultsPanel/ResultsPanel.cpp
    UI/ResultsPanel/ResultsPanel.ui
    UI/ClusteringPanel/ClusteringPanel.hpp
    UI/ClusteringPanel/ClusteringPanel.cpp
    UI/ClusteringPanel/ClusteringPanel.ui
    UI/ClusterOutputPanel/ClusterOutputPanel.hpp
    UI/ClusterOutputPanel/ClusterOutputPanel.cpp
    UI/ClusterOutputPanel/ClusterOutputPanel.ui
    UI/DimReductionPanel/DimReductionPanel.hpp
    UI/DimReductionPanel/DimReductionPanel.cpp
    UI/DimReductionPanel/DimReductionPanel.ui
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
        GroupManagementWidget
        reflectcpp::reflectcpp
)

target_link_libraries(MLCore_Widget PRIVATE WhiskerToolbox::DigitalTimeSeries)
target_link_libraries(MLCore_Widget PRIVATE WhiskerToolbox::TensorData)

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
enable_whiskertoolbox_shared_pch(MLCore_Widget)