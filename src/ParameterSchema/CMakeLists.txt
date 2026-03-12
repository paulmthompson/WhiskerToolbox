find_package(reflectcpp CONFIG REQUIRED)

add_library(ParameterSchema STATIC
    ParameterSchema.hpp
    ParameterSchema.cpp
)

add_library(WhiskerToolbox::ParameterSchema ALIAS ParameterSchema)


# Expose src/ so consumers can include as: #include "ParameterSchema/ParameterSchema.hpp"
target_include_directories(ParameterSchema PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_compiler_warnings(ParameterSchema)

target_link_libraries(ParameterSchema PUBLIC reflectcpp::reflectcpp)
enable_whiskertoolbox_shared_pch(ParameterSchema)