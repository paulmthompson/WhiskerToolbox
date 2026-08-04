# TensorDesign Static Library
# Qt-free lazy-column tensor assembly from JSON design specifications.

find_package(nlohmann_json CONFIG REQUIRED)

add_library(TensorDesign STATIC
    TensorDesignSpec.hpp
    ColumnRecipePresetRegistry.hpp
    ColumnRecipePresetRegistry.cpp
    DesignPresetRegistry.hpp
    DesignPresetRegistry.cpp
    TensorDesignBuilder.hpp
    TensorDesignBuilder.cpp
    TensorColumnViews.hpp
    TensorColumnViews.cpp
    Presets/PresetInterfaces.hpp
    Presets/BuiltInPresets.hpp
    Presets/BuiltInPresets.cpp
    Presets/JsonPresetDto.hpp
    Presets/JsonColumnAggregator.hpp
    Presets/JsonColumnAggregator.cpp
)

target_include_directories(TensorDesign PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_properties(TensorDesign PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(TensorDesign PUBLIC DataManager)
target_link_libraries(TensorDesign PRIVATE PointData)
target_link_libraries(TensorDesign PUBLIC TransformsV2)
target_link_libraries(TensorDesign PUBLIC WhiskerToolbox::TensorData)
target_link_libraries(TensorDesign PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(TensorDesign PRIVATE spdlog::spdlog_header_only)

set_target_compiler_warnings(TensorDesign)

add_library(WhiskerToolbox::TensorDesign ALIAS TensorDesign)
enable_whiskertoolbox_shared_pch(TensorDesign)
