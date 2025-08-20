# ImageProcessing Library CMakeLists.txt

# Create ImageProcessing as a shared library
set(IMAGEPROCESSING_SOURCES
    src/OpenCVUtility.cpp
)

set(IMAGEPROCESSING_INCLUDES
    include/ImageProcessing/ProcessingOptions.hpp
    include/ImageProcessing/OpenCVUtility.hpp
)

# Create the ImageProcessing shared library
add_library(ImageProcessing SHARED ${IMAGEPROCESSING_SOURCES})

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

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ImageProcessing PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ImageProcessing PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(ImageProcessing PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(ImageProcessing PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

install(FILES ${IMAGEPROCESSING_INCLUDES}
    DESTINATION include/ImageProcessing
)