
# MaskData Library CMakeLists.txt

# Create MaskData as a separate shared library including all IO formats
set(MASKDATA_SOURCES
    Mask_Data.hpp
    Mask_Data.cpp
    IO/Image/Mask_Data_Image.hpp
    IO/Image/Mask_Data_Image.cpp
    IO/JSON/Mask_Data_JSON.hpp
    IO/JSON/Mask_Data_JSON.cpp
    utils/connected_component.hpp
    utils/connected_component.cpp
    utils/hole_filling.hpp
    utils/hole_filling.cpp
    utils/median_filter.hpp
    utils/median_filter.cpp
    utils/Image.hpp
    utils/Image.cpp
    utils/mask_utils.hpp
    utils/mask_utils.cpp
    utils/skeletonize.cpp
    utils/skeletonize.hpp
)

# Create the MaskData shared library
add_library(MaskData SHARED ${MASKDATA_SOURCES})

# Set up include directories
target_include_directories(MaskData PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For DataManager/utils and other utils
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>     # For IO subdirectories
    $<INSTALL_INTERFACE:include>
)

# Link to required libraries
target_link_libraries(MaskData PUBLIC
    NEURALYZER_GEOMETRY      # CoreGeometry
    WhiskerToolbox::ObserverData  # Observer
    TimeFrame                # TimeFrame  
    Entity                   # Entity
    nlohmann_json::nlohmann_json  # For JSON support
    opencv_core opencv_imgproc opencv_imgcodecs opencv_photo  # For Image processing
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MaskData PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(MaskData PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(MaskData PRIVATE ${MSVC_WARNINGS})
endif()

# Set properties
set_target_properties(MaskData PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    VERSION 1.0.0
    SOVERSION 1
)

# Create alias for consistent naming
add_library(WhiskerToolbox::MaskData ALIAS MaskData)

# Test sources
set(masks_test_sources
    Mask_Data.test.cpp
    utils/connected_component.test.cpp
    utils/hole_filling.test.cpp
    utils/median_filter.test.cpp
    utils/Image.test.cpp
    utils/skeletonize.test.cpp
    utils/mask_utils.test.cpp
)

add_tests_to_global(masks_test_sources 
                    "${CMAKE_SOURCE_DIR}/src/DataManager/Masks" 
                    DATAMANAGER_TEST_SOURCES)

# No longer add sources to DataManager since they're now in the MaskData library
