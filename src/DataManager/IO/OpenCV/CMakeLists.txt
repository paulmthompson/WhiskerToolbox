find_package(OpenCV CONFIG REQUIRED)

# Create DataManagerOpenCV as a separate shared library for OpenCV utilities
set(DATAMANAGER_OPENCV_SOURCES
    OpenCVLoader.hpp
    OpenCVLoader.cpp
    OpenCVFormatLoader.hpp
    OpenCVFormatLoader.cpp
    OpenCVInit.hpp
    OpenCVInit.cpp
    Mask_Data_Image.hpp
    Mask_Data_Image.cpp
)

# Create the DataManagerOpenCV shared library
add_library(DataManagerOpenCV SHARED ${DATAMANAGER_OPENCV_SOURCES})

# Set up include directories
target_include_directories(DataManagerOpenCV PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Find and link OpenCV
find_package(OpenCV CONFIG REQUIRED)

# Link to required libraries
target_link_libraries(DataManagerOpenCV PRIVATE
    DataManagerIO            # For LoaderRegistry access
    NEURALYZER_GEOMETRY      # CoreGeometry for Point2D
    nlohmann_json::nlohmann_json  # For JSON config parsing
    WhiskerToolbox::MaskData
    opencv_core opencv_imgproc opencv_imgcodecs  # OpenCV components
)

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

# Set properties
set_target_properties(DataManagerOpenCV PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    VERSION 1.0.0
    SOVERSION 1
)

# Create alias for consistent naming
add_library(WhiskerToolbox::DataManagerOpenCV ALIAS DataManagerOpenCV)
