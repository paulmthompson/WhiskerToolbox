find_package(nlohmann_json CONFIG REQUIRED)

find_package(OpenCV CONFIG REQUIRED)

# Create DataManagerOpenCV as a separate shared library for OpenCV utilities
set(DATAMANAGER_OPENCV_SOURCES
    OpenCVLoader.hpp
    OpenCVLoader.cpp
    OpenCVFormatLoader.hpp
    OpenCVFormatLoader.cpp
    OpenCVInit.hpp
    OpenCVInit.cpp
    maskdata/Mask_Data_Image.hpp
    maskdata/Mask_Data_Image.cpp
)

add_library(DataManagerOpenCV SHARED ${DATAMANAGER_OPENCV_SOURCES})

include(GenerateExportHeader)

set_target_properties(DataManagerOpenCV
    PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

generate_export_header(DataManagerOpenCV
                       EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/datamanagerio_opencv_export.h
                       EXPORT_MACRO_NAME DATAMANAGERIO_OPENCV_EXPORT
                       NO_EXPORT_MACRO_NAME DATAMANAGERIO_OPENCV_NO_EXPORT
                       STATIC_DEFINE DATAMANAGERIO_OPENCV_STATIC_DEFINE)

target_include_directories(DataManagerOpenCV PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>" 
    $<INSTALL_INTERFACE:include>
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR} # For generated export header
)

target_sources(
  DataManagerOpenCV PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${CMAKE_CURRENT_BINARY_DIR}/datamanagerio_opencv_export.h
)


find_package(OpenCV CONFIG REQUIRED)

# Link to required libraries
target_link_libraries(DataManagerOpenCV PRIVATE
 #   DataManagerIO            # For LoaderRegistry access
    DataManagerIO_Core       # For IFormatLoader interface
    NEURALYZER_GEOMETRY      # CoreGeometry for Point2D
    nlohmann_json::nlohmann_json  # For JSON config parsing
    ParameterSchema
    DataTypeEnum
    WhiskerToolbox::MaskData
    opencv_core opencv_imgproc opencv_imgcodecs  # OpenCV components
    spdlog::spdlog
)

target_link_libraries(DataManagerOpenCV PRIVATE CoreUtilities)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataManagerOpenCV PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataManagerOpenCV PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataManagerOpenCV PRIVATE ${MSVC_WARNINGS})
endif()

# Prevent static-archive symbols (e.g. TimeFrame) from leaking into the .so
target_link_options(DataManagerOpenCV PRIVATE
    $<$<CXX_COMPILER_ID:Clang,GNU>:LINKER:--exclude-libs,ALL>)

# Set properties
set_target_properties(DataManagerOpenCV PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    VERSION 1.0.0
    SOVERSION 1
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Create alias for consistent naming
add_library(WhiskerToolbox::DataManagerOpenCV ALIAS DataManagerOpenCV)
enable_whiskertoolbox_pch(DataManagerOpenCV)
