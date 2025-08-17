# ObserverData Static Library
# This library provides the observer pattern implementation for data classes

add_library(ObserverData STATIC
    Observer_Data.hpp
    Observer_Data.cpp
)

# Set up include directories
target_include_directories(ObserverData PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Apply the same compiler flags as the main project
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ObserverData PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ObserverData PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(ObserverData PRIVATE ${MSVC_WARNINGS})
endif()

# Set target properties to match the main project
set_target_properties(ObserverData PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add alias for consistent naming
add_library(WhiskerToolbox::ObserverData ALIAS ObserverData)

# Note: Observer test is built from the main test directory to ensure Catch2 is available
# The test file Observer_Data.test.cpp is kept here for co-location with implementation


