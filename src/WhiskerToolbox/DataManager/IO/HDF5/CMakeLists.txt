# DataManagerHDF5 Library CMakeLists.txt

# Create DataManagerHDF5 as a separate shared library for HDF5 utilities
set(DATAMANAGER_HDF5_SOURCES
    hdf5_utilities.hpp
    hdf5_utilities.cpp
    hdf5_loaders.hpp
    hdf5_loaders.cpp
)

# Create the DataManagerHDF5 shared library
add_library(DataManagerHDF5 SHARED ${DATAMANAGER_HDF5_SOURCES})

# Set up include directories
target_include_directories(DataManagerHDF5 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Find and link HDF5
find_package(HDF5 COMPONENTS CXX REQUIRED)

# Link to required libraries
target_link_libraries(DataManagerHDF5 PUBLIC
    NEURALYZER_GEOMETRY      # CoreGeometry for Point2D
)

# Platform-specific HDF5 linking
if (APPLE)
    target_link_libraries(DataManagerHDF5 PRIVATE hdf5::hdf5-static hdf5::hdf5_cpp-static)
else()
    target_link_libraries(DataManagerHDF5 PRIVATE hdf5::hdf5-shared hdf5::hdf5_cpp-shared)
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
