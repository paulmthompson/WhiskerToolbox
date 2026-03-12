
# LineData Library CMakeLists.txt

# Create LineData as a separate shared library including all IO formats (except Binary/CapnProto)
set(LINEDATA_SOURCES
    Line_Data.hpp
    Line_Data.cpp
    #IO/RocksDB/Line_Data_RocksDB.hpp  # Commented out - may depend on CapnProto
    #IO/RocksDB/Line_Data_RocksDB.cpp
    #IO/LMDB/Line_Data_LMDB.hpp        # Commented out - may depend on CapnProto
    #IO/LMDB/Line_Data_LMDB.cpp
)

# Create the LineData static library
add_library(LineData STATIC ${LINEDATA_SOURCES})

# Set up include directories
target_include_directories(LineData PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For DataManager/utils/map_timeseries.hpp and other utils
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>     # For IO subdirectories
    $<INSTALL_INTERFACE:include>
)

# Link to required libraries
target_link_libraries(LineData PUBLIC
    NEURALYZER_GEOMETRY      # CoreGeometry
    WhiskerToolbox::ObserverData  # Observer
    TimeFrame                # TimeFrame  
    Entity                   # Entity
    DataTypeTraits           # DataTypeTraits
    RaggedTimeSeries         # RaggedTimeSeries
)

set_target_compiler_warnings(LineData)

# Set properties
set_target_properties(LineData PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Create alias for consistent naming
add_library(WhiskerToolbox::LineData ALIAS LineData)

# Test sources
set(lines_test_sources
    Line_Data.test.cpp
)

add_tests_to_global(lines_test_sources 
                    "${CMAKE_SOURCE_DIR}/src/DataManager/Lines" 
                    DATAMANAGER_TEST_SOURCES)

enable_whiskertoolbox_pch(LineData)
