# SpatialIndex Library
# This library provides spatial indexing utilities

add_library(SpatialIndex STATIC
    KdTree.cpp
)

# Add the interface headers
target_sources(SpatialIndex PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/QuadTree.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/RTree.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/KdTree.hpp>
    $<INSTALL_INTERFACE:SpatialIndex/QuadTree.hpp>
    $<INSTALL_INTERFACE:SpatialIndex/RTree.hpp>
    $<INSTALL_INTERFACE:SpatialIndex/KdTree.hpp>
)

# Specify include directories
target_include_directories(SpatialIndex PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/SpatialIndex>
)

target_link_libraries(SpatialIndex PUBLIC NEURALYZER_GEOMETRY)
