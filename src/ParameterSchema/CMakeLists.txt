find_package(reflectcpp CONFIG REQUIRED)

add_library(ParameterSchema STATIC
    ParameterSchema.hpp
    ParameterSchema.cpp
)

add_library(WhiskerToolbox::ParameterSchema ALIAS ParameterSchema)

set_target_properties(ParameterSchema
    PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

# Expose src/ so consumers can include as: #include "ParameterSchema/ParameterSchema.hpp"
target_include_directories(ParameterSchema PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Apply the same compiler flags as the main project
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ParameterSchema PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ParameterSchema PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(ParameterSchema PRIVATE ${MSVC_WARNINGS})
endif()

target_link_libraries(ParameterSchema PUBLIC reflectcpp::reflectcpp)
enable_whiskertoolbox_shared_pch(ParameterSchema)