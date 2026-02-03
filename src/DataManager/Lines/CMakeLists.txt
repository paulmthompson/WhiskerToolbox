
# LineData Library CMakeLists.txt

# Create LineData as a separate shared library including all IO formats (except Binary/CapnProto)
set(LINEDATA_SOURCES
    Line_Data.hpp
    Line_Data.cpp
    IO/CSV/Line_Data_CSV.hpp
    IO/CSV/Line_Data_CSV.cpp
    IO/JSON/Line_Data_JSON.hpp
    IO/JSON/Line_Data_JSON.cpp
    #IO/RocksDB/Line_Data_RocksDB.hpp  # Commented out - may depend on CapnProto
    #IO/RocksDB/Line_Data_RocksDB.cpp
    #IO/LMDB/Line_Data_LMDB.hpp        # Commented out - may depend on CapnProto
    #IO/LMDB/Line_Data_LMDB.cpp
)

# Create the LineData shared library
add_library(LineData SHARED ${LINEDATA_SOURCES})

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
    nlohmann_json::nlohmann_json  # For JSON support
    reflectcpp::reflectcpp
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(LineData PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(LineData PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(LineData PRIVATE ${MSVC_WARNINGS})
endif()

# Set properties
set_target_properties(LineData PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    VERSION 1.0.0
    SOVERSION 1
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

# No longer add sources to DataManager since they're now in the LineData library
