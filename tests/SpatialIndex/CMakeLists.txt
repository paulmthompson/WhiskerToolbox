if (APPLE)
    message(STATUS "Testing Currenly not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currenly not supported on Windows")
    return()
endif()

# Test executable for SpatialIndex
add_executable(SpatialIndexTests
    ${CMAKE_SOURCE_DIR}/src/SpatialIndex/QuadTree.test.cpp
    ${CMAKE_SOURCE_DIR}/src/SpatialIndex/RTree.test.cpp
    ${CMAKE_SOURCE_DIR}/src/SpatialIndex/KdTree.test.cpp
)

# Link against our SpatialIndex library and Catch2
target_link_libraries(SpatialIndexTests 
    PRIVATE 
    SpatialIndex
    NEURALYZER_GEOMETRY
    Catch2::Catch2WithMain
)

# Add test to CTest
catch_discover_tests(SpatialIndexTests)
