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

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(SpatialIndexTests PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(SpatialIndexTests PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(SpatialIndexTests PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_test_pch(SpatialIndexTests)