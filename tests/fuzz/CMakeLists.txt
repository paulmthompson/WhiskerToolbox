# Fuzzing tests for WhiskerToolbox

if(NOT ENABLE_FUZZING)
    message(STATUS "Fuzzing is disabled. Set ENABLE_FUZZING=ON to enable fuzz tests.")
    return()
endif()

message(STATUS "Building fuzz tests...")

# Set the output directory for fuzz test binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

# Fuzz target 1: Analog CSV parser
add_fuzz_target(fuzz_analog_csv
    fuzz_analog_csv.cpp
    LINK_LIBRARIES
        DataManagerIO
        DataManager
    INCLUDE_DIRECTORIES
        ${CMAKE_SOURCE_DIR}/src/DataManager
        ${CMAKE_SOURCE_DIR}/src
)

# Fuzz target 2: Point CSV parser
add_fuzz_target(fuzz_point_csv
    fuzz_point_csv.cpp
    LINK_LIBRARIES
        DataManagerIO
        DataManager
    INCLUDE_DIRECTORIES
        ${CMAKE_SOURCE_DIR}/src/DataManager
        ${CMAKE_SOURCE_DIR}/src
)

# Fuzz target 3: Point JSON parser
add_fuzz_target(fuzz_point_json
    fuzz_point_json.cpp
    LINK_LIBRARIES
        DataManagerIO
        DataManager
    INCLUDE_DIRECTORIES
        ${CMAKE_SOURCE_DIR}/src/DataManager
        ${CMAKE_SOURCE_DIR}/src
)

# Fuzz target 4: Line CSV parser
add_fuzz_target(fuzz_line_csv
    fuzz_line_csv.cpp
    LINK_LIBRARIES
        DataManagerIO
        DataManager
    INCLUDE_DIRECTORIES
        ${CMAKE_SOURCE_DIR}/src/DataManager
        ${CMAKE_SOURCE_DIR}/src
)

message(STATUS "Fuzz tests configured. Use 'make' to build them.")
message(STATUS "Run fuzz tests with: ./bin/fuzz_<target_name> -max_total_time=60")
