find_package(reflectcpp CONFIG REQUIRED)

add_library(MediaProcessingPipeline STATIC
    ProcessingOptionsSchema.hpp
    ProcessingStep.hpp
    ProcessingStepRegistry.hpp
    ProcessingStepRegistry.cpp
)

add_library(WhiskerToolbox::MediaProcessingPipeline ALIAS MediaProcessingPipeline)

target_include_directories(MediaProcessingPipeline PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(MediaProcessingPipeline PUBLIC
    WhiskerToolbox::ParameterSchema
    ImageProcessing
    reflectcpp::reflectcpp
)

set_target_compiler_warnings(MediaProcessingPipeline)
enable_whiskertoolbox_shared_pch(MediaProcessingPipeline)
