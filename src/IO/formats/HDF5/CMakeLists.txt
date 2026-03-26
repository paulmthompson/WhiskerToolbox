find_package(HDF5 COMPONENTS CXX REQUIRED)

find_package(nlohmann_json CONFIG REQUIRED)

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

add_library(DataManagerHDF5 SHARED ${DATAMANAGER_HDF5_SOURCES})

include(GenerateExportHeader)

set_target_properties(DataManagerHDF5
    PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

generate_export_header(DataManagerHDF5
                       EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/datamanagerio_hdf5_export.h
                       EXPORT_MACRO_NAME DATAMANAGERIO_HDF5_EXPORT
                       NO_EXPORT_MACRO_NAME DATAMANAGERIO_HDF5_NO_EXPORT
                       STATIC_DEFINE DATAMANAGERIO_HDF5_STATIC_DEFINE)


# Set up include directories
target_include_directories(DataManagerHDF5 PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>" 
    $<INSTALL_INTERFACE:include>
    PUBLIC 
    ${CMAKE_CURRENT_BINARY_DIR} # For generated export header
)

target_sources(
  DataManagerHDF5 PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${CMAKE_CURRENT_BINARY_DIR}/datamanagerio_hdf5_export.h
)


# Find and link HDF5
find_package(HDF5 COMPONENTS CXX REQUIRED)

# Link to required libraries
target_link_libraries(DataManagerHDF5 PRIVATE
#    DataManagerIO            # For LoaderRegistry access
    DataManagerIO_Core       # For IFormatLoader interface
    NEURALYZER_GEOMETRY      # CoreGeometry for Point2D
    nlohmann_json::nlohmann_json  # For JSON config parsing
    DataTypeEnum
    WhiskerToolbox::LineData
    WhiskerToolbox::MaskData
    WhiskerToolbox::DigitalTimeSeries   # For DigitalEventSeries
    WhiskerToolbox::AnalogTimeSeries    # For AnalogTimeSeries
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

# Prevent static-archive symbols (e.g. TimeFrame) from leaking into the .so
target_link_options(DataManagerHDF5 PRIVATE
    $<$<CXX_COMPILER_ID:Clang,GNU>:LINKER:--exclude-libs,ALL>)

# Set properties
set_target_properties(DataManagerHDF5 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    VERSION 1.0.0
    SOVERSION 1
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Create alias for consistent naming
add_library(WhiskerToolbox::DataManagerHDF5 ALIAS DataManagerHDF5)
enable_whiskertoolbox_pch(DataManagerHDF5)