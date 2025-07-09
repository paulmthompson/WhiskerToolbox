
# Test executable for SpatialIndex
add_executable(SpatialIndexTests
    QuadTree.test.cpp
)

# Link against our SpatialIndex library and Catch2
target_link_libraries(SpatialIndexTests 
    PRIVATE 
    SpatialIndex
    Catch2::Catch2WithMain
)

# Add test to CTest
catch_discover_tests(SpatialIndexTests)
