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


add_library(DataManagerNumpy SHARED ${DATAMANAGER_NUMPY_SOURCES})

include(GenerateExportHeader)

set_target_properties(DataManagerNumpy
    PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

generate_export_header(DataManagerNumpy
                       EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/datamanagerio_numpy_export.h
                       EXPORT_MACRO_NAME DATAMANAGERIO_NUMPY_EXPORT
                       NO_EXPORT_MACRO_NAME DATAMANAGERIO_NUMPY_NO_EXPORT
                       STATIC_DEFINE DATAMANAGERIO_NUMPY_STATIC_DEFINE)

# Set up include directories
target_include_directories(DataManagerNumpy PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>"  #I'll make this better i Promise
    $<INSTALL_INTERFACE:include>
    "${npy_SOURCE_DIR}/include"  # For libnpy header-only library
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR} # For generated export header
    )

target_sources(
  DataManagerNumpy PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${CMAKE_CURRENT_BINARY_DIR}/datamanagerio_numpy_export.h
)


# Link to required libraries
target_link_libraries(DataManagerNumpy PRIVATE
    DataManagerIO_Core        # For IFormatLoader interface
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

# Prevent static-archive symbols (e.g. TimeFrame) from leaking into the .so
target_link_options(DataManagerNumpy PRIVATE
    $<$<CXX_COMPILER_ID:Clang,GNU>:LINKER:--exclude-libs,ALL>)

# Set properties
set_target_properties(DataManagerNumpy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    VERSION 1.0.0
    SOVERSION 1
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Create alias for consistent naming
add_library(WhiskerToolbox::DataManagerNumpy ALIAS DataManagerNumpy)
enable_whiskertoolbox_pch(DataManagerNumpy)