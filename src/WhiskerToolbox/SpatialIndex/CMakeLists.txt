# SpatialIndex Interface Library
# This library provides header-only spatial indexing utilities

add_library(SpatialIndex INTERFACE)

# Add the interface headers
target_sources(SpatialIndex INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/QuadTree.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/RTree.hpp>
    $<INSTALL_INTERFACE:SpatialIndex/QuadTree.hpp>
    $<INSTALL_INTERFACE:SpatialIndex/RTree.hpp>
)

# Specify include directories
target_include_directories(SpatialIndex INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/SpatialIndex>
)

target_link_libraries(SpatialIndex INTERFACE NEURALYZER_GEOMETRY)

# Set up export target
install(TARGETS SpatialIndex
    EXPORT SpatialIndexTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install headers
install(FILES QuadTree.hpp RTree.hpp
    DESTINATION include/SpatialIndex
)

# Export target configuration
install(EXPORT SpatialIndexTargets
    FILE SpatialIndexTargets.cmake
    NAMESPACE SpatialIndex::
    DESTINATION lib/cmake/SpatialIndex
)
