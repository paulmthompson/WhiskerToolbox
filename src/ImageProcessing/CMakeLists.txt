# ImageProcessing Library CMakeLists.txt

# Create ImageProcessing as a static library
set(IMAGEPROCESSING_SOURCES
    src/OpenCVUtility.cpp
)

set(IMAGEPROCESSING_INCLUDES
    include/ImageProcessing/ProcessingOptions.hpp
    include/ImageProcessing/OpenCVUtility.hpp
)

add_library(ImageProcessing STATIC ${IMAGEPROCESSING_SOURCES})

# Find OpenCV
find_package(OpenCV CONFIG REQUIRED)

# Include directories
target_include_directories(ImageProcessing PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
    ${OpenCV_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(ImageProcessing PUBLIC
    NEURALYZER_GEOMETRY
    ${OpenCV_LIBS}
)

set_target_compiler_warnings(ImageProcessing)

set_target_properties(ImageProcessing PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
enable_whiskertoolbox_pch(ImageProcessing)