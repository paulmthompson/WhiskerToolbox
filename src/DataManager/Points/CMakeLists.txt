
set(points_sources
    Point_Data.hpp
    Point_Data.cpp
    utils/Point_Data_utils.hpp
    utils/Point_Data_utils.cpp
)

# Create the PointData static library
add_library(PointData STATIC ${points_sources})
set_target_properties(PointData PROPERTIES POSITION_INDEPENDENT_CODE ON)


target_include_directories(PointData PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For DataManager/utils and other utils
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>     # For IO subdirectories
    $<INSTALL_INTERFACE:include>
)

# Link to required libraries
target_link_libraries(PointData PUBLIC
    NEURALYZER_GEOMETRY      # CoreGeometry
    WhiskerToolbox::ObserverData  # Observer
    TimeFrame                # TimeFrame  
    Entity                   # Entity
    DataTypeTraits           # DataTypeTraits
    RaggedTimeSeries         # RaggedTimeSeries
)

set_target_compiler_warnings(PointData)

# Create alias for consistent naming
add_library(WhiskerToolbox::PointData ALIAS PointData)

set(points_test_sources
    Point_Data.test.cpp
)

add_tests_to_global(points_test_sources
                    "${CMAKE_SOURCE_DIR}/src/DataManager/Points"
                    DATAMANAGER_TEST_SOURCES)
enable_whiskertoolbox_pch(PointData)