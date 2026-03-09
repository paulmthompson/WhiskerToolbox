find_package(Armadillo CONFIG REQUIRED)

set(NEURALYZER_COREMATH_SOURCES
    parametric_polynomial_utils.cpp
    parametric_polynomial_utils.hpp
    polynomial_fit.cpp
    polynomial_fit.hpp
)

add_library(CoreMath STATIC ${NEURALYZER_COREMATH_SOURCES})
set_target_properties(CoreMath PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Set up include directories
target_include_directories(CoreMath PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"  # For "CoreGeometry/" includes
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(CoreMath PRIVATE NEURALYZER_GEOMETRY armadillo)

# Apply the same compiler flags as the main project
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(CoreMath PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(CoreMath PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(CoreMath PRIVATE ${MSVC_WARNINGS})
endif()

# Set target properties to match the main project
set_target_properties(CoreMath PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add alias for consistent naming
add_library(WhiskerToolbox::CoreMath ALIAS CoreMath)
enable_whiskertoolbox_pch(CoreMath)