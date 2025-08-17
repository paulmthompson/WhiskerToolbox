# MediaData Library CMakeLists.txt

# Find required packages conditionally
if(ENABLE_HDF5)
    find_package(HDF5 COMPONENTS CXX REQUIRED)
endif()

# Create MediaData as a separate shared library including all IO formats
set(MEDIADATA_SOURCES
    Media_Data.hpp
    Media_Data.cpp
    Video_Data.hpp
    Video_Data.cpp
    Image_Data.hpp
    Image_Data.cpp
    Video_Data_Loader.hpp
    IO/Image_Data_Loader.hpp
    IO/Image_Data_Loader.cpp
    IO/JSON/Image_Data_JSON.hpp
    IO/JSON/Image_Data_JSON.cpp
    utils/opencv_utility.hpp
    utils/opencv_utility.cpp
)

# Add HDF5-specific sources conditionally
if(ENABLE_HDF5)
    list(APPEND MEDIADATA_SOURCES
        HDF5_Data.hpp
        HDF5_Data.cpp
    )
endif()

# Create the MediaData shared library
add_library(MediaData SHARED ${MEDIADATA_SOURCES})

# Set up include directories
target_include_directories(MediaData PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For DataManager/utils and other utils
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>     # For IO subdirectories
    $<INSTALL_INTERFACE:include>
)

# Link to required libraries
target_link_libraries(MediaData PUBLIC
    NEURALYZER_GEOMETRY      # CoreGeometry
    WhiskerToolbox::ObserverData  # Observer
    TimeFrame                # TimeFrame  
    Entity                   # Entity
    nlohmann_json::nlohmann_json  # For JSON support
    opencv_core opencv_imgproc opencv_imgcodecs opencv_photo  # For Image processing
    ffmpeg_wrapper::ffmpeg_wrapper  # For Video support
)

# Link HDF5 libraries conditionally
if(ENABLE_HDF5)
    if (APPLE)
        target_link_libraries(MediaData PRIVATE hdf5::hdf5-static hdf5::hdf5_cpp-static)
    else()
        target_link_libraries(MediaData PRIVATE hdf5::hdf5-shared hdf5::hdf5_cpp-shared)
    endif()
    # Add HDF5 compile definition for conditional compilation
    target_compile_definitions(MediaData PRIVATE ENABLE_HDF5)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MediaData PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(MediaData PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(MediaData PRIVATE ${MSVC_WARNINGS})
endif()

# Set properties
set_target_properties(MediaData PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    VERSION 1.0.0
    SOVERSION 1
)

# Create alias for consistent naming
add_library(WhiskerToolbox::MediaData ALIAS MediaData)

# Test sources
set(MEDIADATA_TEST_SOURCES
    # Add test sources here if they exist
)

# Add to global test list (following the pattern from Lines/CMakeLists.txt)
get_property(test_sources GLOBAL PROPERTY DATAMANAGER_TEST_SOURCES)
set_property(GLOBAL PROPERTY DATAMANAGER_TEST_SOURCES "${test_sources};${MEDIADATA_TEST_SOURCES}")
