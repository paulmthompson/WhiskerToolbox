find_package(HDF5 COMPONENTS CXX REQUIRED)

# Create DataManagerHDF5 as a separate shared library for HDF5 utilities
set(DATAMANAGER_HDF5_SOURCES
    common/hdf5_utilities.hpp
    common/hdf5_utilities.cpp
    hdf5_loaders.hpp
    hdf5_loaders.cpp
    HDF5Loader.hpp
    HDF5Loader.cpp
    HDF5FormatLoader.hpp
    HDF5FormatLoader.cpp
    HDF5Init.hpp
    HDF5Init.cpp
    maskdata/Mask_Data_HDF5.hpp
    maskdata/Mask_Data_HDF5.cpp
)

# Create the DataManagerHDF5 shared library
add_library(DataManagerHDF5 SHARED ${DATAMANAGER_HDF5_SOURCES})

# Set up include directories
target_include_directories(DataManagerHDF5 PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Find and link HDF5
find_package(HDF5 COMPONENTS CXX REQUIRED)

# Link to required libraries
target_link_libraries(DataManagerHDF5 PRIVATE
#    DataManagerIO            # For LoaderRegistry access
    NEURALYZER_GEOMETRY      # CoreGeometry for Point2D
    nlohmann_json::nlohmann_json  # For JSON config parsing
    WhiskerToolbox::LineData
    WhiskerToolbox::MaskData
)

# Platform-specific HDF5 linking
if (APPLE)
    target_link_libraries(DataManagerHDF5 PRIVATE hdf5::hdf5-static hdf5::hdf5_cpp-static)
else()
    target_link_libraries(DataManagerHDF5 PRIVATE hdf5::hdf5-shared hdf5::hdf5_cpp-shared)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataManagerHDF5 PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataManagerHDF5 PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataManagerHDF5 PRIVATE ${MSVC_WARNINGS})
endif()

# Set properties
set_target_properties(DataManagerHDF5 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    VERSION 1.0.0
    SOVERSION 1
)

# Create alias for consistent naming
add_library(WhiskerToolbox::DataManagerHDF5 ALIAS DataManagerHDF5)
