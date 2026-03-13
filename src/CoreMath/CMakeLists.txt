find_package(Armadillo CONFIG REQUIRED)

set(NEURALYZER_COREMATH_SOURCES
    parametric_polynomial_utils.cpp
    parametric_polynomial_utils.hpp
    polynomial_fit.cpp
    polynomial_fit.hpp
)

add_library(CoreMath STATIC ${NEURALYZER_COREMATH_SOURCES})

target_include_directories(CoreMath PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"  # For "CoreGeometry/" includes
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(CoreMath PRIVATE NEURALYZER_GEOMETRY armadillo)

set_target_compiler_warnings(CoreMath)

set_target_properties(CoreMath PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_library(WhiskerToolbox::CoreMath ALIAS CoreMath)
enable_whiskertoolbox_shared_pch(CoreMath)