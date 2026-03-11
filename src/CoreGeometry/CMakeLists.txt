# NEURALYZER_GEOMETRY
# This library provides header-only geometry utilities

set(NEURALYZER_GEOMETRY_INCLUDES
    include/CoreGeometry/angle.hpp
    include/CoreGeometry/boundingbox.hpp
    include/CoreGeometry/geometry.hpp
    include/CoreGeometry/Image.hpp
    include/CoreGeometry/ImageSize.hpp
    include/CoreGeometry/lines.hpp
    include/CoreGeometry/line_geometry.hpp
    include/CoreGeometry/order_line.hpp
    include/CoreGeometry/line_resampling.hpp
    include/CoreGeometry/masks.hpp
    include/CoreGeometry/points.hpp
    include/CoreGeometry/point_geometry.hpp
    include/CoreGeometry/polygon.hpp
    include/CoreGeometry/polygon_adapter.hpp
    include/CoreGeometry/bop12/bbox_2.h
    include/CoreGeometry/bop12/booleanop.h
    include/CoreGeometry/bop12/point_2.h 
    include/CoreGeometry/bop12/polygon.h 
    include/CoreGeometry/bop12/segment_2.h 
    include/CoreGeometry/bop12/utilities.h 
)

set(NEURALYZER_GEOMETRY_SOURCES
   src/angle.cpp
   src/Image.cpp
   src/lines.cpp
   src/line_geometry.cpp
   src/order_line.cpp
   src/line_resampling.cpp
   src/masks.cpp
   src/points.cpp
   src/point_geometry.cpp
   src/polygon.cpp
   src/polygon_adapter.cpp
   src/bop12/booleanop.cpp
   src/bop12/polygon.cpp 
   src/bop12/utilities.cpp
)

set_property(GLOBAL PROPERTY CORE_GEOMETRY_TEST_SOURCES )

set(core_geometry_test_sources
    src/Image.test.cpp
    src/line_geometry.test.cpp
    src/masks.test.cpp
    src/polygon.test.cpp
)

set(CORE_GEOMETRY_TEST_SOURCES ${core_geometry_test_sources} PARENT_SCOPE)

add_tests_to_global(core_geometry_test_sources
                    "${CMAKE_SOURCE_DIR}/src/CoreGeometry"
                    CORE_GEOMETRY_TEST_SOURCES)


add_library(NEURALYZER_GEOMETRY STATIC ${NEURALYZER_GEOMETRY_SOURCES})


target_include_directories(NEURALYZER_GEOMETRY PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

set_target_compiler_warnings(NEURALYZER_GEOMETRY)

set_target_properties(NEURALYZER_GEOMETRY PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

#enable_whiskertoolbox_pch(NEURALYZER_GEOMETRY) # Not worth it, this compiles fast