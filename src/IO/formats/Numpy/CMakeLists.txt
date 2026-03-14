# Numpy Plugin CMakeLists.txt

find_package(nlohmann_json CONFIG REQUIRED)

# Create DataManagerNumpy as a separate shared library for numpy loading
set(DATAMANAGER_NUMPY_SOURCES
    NumpyLoader.hpp
    NumpyLoader.cpp
    NumpyFormatLoader.hpp
    NumpyFormatLoader.cpp
    tensordata/Tensor_Data_numpy.hpp
    tensordata/Tensor_Data_numpy.cpp
)

# Create the DataManagerNumpy shared library
add_library(DataManagerNumpy SHARED ${DATAMANAGER_NUMPY_SOURCES})

# Set up include directories
target_include_directories(DataManagerNumpy PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>"  #I'll make this better i Promise
    $<INSTALL_INTERFACE:include>
    "${npy_SOURCE_DIR}/include"  # For libnpy header-only library
)

# Link to required libraries
target_link_libraries(DataManagerNumpy PRIVATE
    nlohmann_json::nlohmann_json  # For JSON config parsing
    WhiskerToolbox::TensorData    # For TensorData
    DataTypeEnum
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataManagerNumpy PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataManagerNumpy PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataManagerNumpy PRIVATE ${MSVC_WARNINGS})
endif()

# Set properties
set_target_properties(DataManagerNumpy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    VERSION 1.0.0
    SOVERSION 1
)

# Create alias for consistent naming
add_library(WhiskerToolbox::DataManagerNumpy ALIAS DataManagerNumpy)
enable_whiskertoolbox_pch(DataManagerNumpy)