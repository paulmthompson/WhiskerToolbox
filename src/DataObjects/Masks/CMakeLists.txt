
# MaskData Library CMakeLists.txt

# Create MaskData as a separate shared library including all IO formats
set(MASKDATA_SOURCES
    Mask_Data.hpp
    Mask_Data.cpp
    utils/connected_component.hpp
    utils/connected_component.cpp
    utils/hole_filling.hpp
    utils/hole_filling.cpp
    utils/median_filter.hpp
    utils/median_filter.cpp
    utils/mask_utils.hpp
    utils/mask_utils.cpp
    utils/skeletonize.cpp
    utils/skeletonize.hpp
)

add_library(MaskData STATIC ${MASKDATA_SOURCES})

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
    DataTypeTraits           # DataTypeTraits
    RaggedTimeSeries         # RaggedTimeSeries
)

set_target_compiler_warnings(MaskData)

# Set properties
set_target_properties(MaskData PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_library(WhiskerToolbox::MaskData ALIAS MaskData)


enable_whiskertoolbox_shared_pch(MaskData)
