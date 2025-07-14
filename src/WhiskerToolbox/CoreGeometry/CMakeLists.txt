# NEURALYZER_GEOMETRY
# This library provides header-only geometry utilities

set(NEURALYZER_GEOMETRY_INCLUDES
    include/CoreGeometry/boundingbox.hpp
    include/CoreGeometry/geometry.hpp
    include/CoreGeometry/ImageSize.hpp
    include/CoreGeometry/masks.hpp
    include/CoreGeometry/points.hpp
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
   src/masks.cpp
   src/points.cpp
   src/polygon.cpp
   src/polygon_adapter.cpp
   src/bop12/booleanop.cpp
   src/bop12/polygon.cpp 
   src/bop12/utilities.cpp
)

set_property(GLOBAL PROPERTY CORE_GEOMETRY_TEST_SOURCES )

set(core_geometry_test_sources
    src/masks.test.cpp
    src/polygon.test.cpp
)

set(CORE_GEOMETRY_TEST_SOURCES ${core_geometry_test_sources} PARENT_SCOPE)

add_tests_to_global(core_geometry_test_sources
                    "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/CoreGeometry"
                    CORE_GEOMETRY_TEST_SOURCES)


add_library(NEURALYZER_GEOMETRY SHARED ${NEURALYZER_GEOMETRY_SOURCES})


target_include_directories(NEURALYZER_GEOMETRY PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

install(TARGETS NEURALYZER_GEOMETRY)

# Set up export target
install(TARGETS NEURALYZER_GEOMETRY
    EXPORT NEURALYZER_GEOMETRYTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES ${NEURALYZER_GEOMETRY_INCLUDES}
    DESTINATION include/CoreGeometry
)

# Export target configuration
install(EXPORT NEURALYZER_GEOMETRYTargets
    FILE NEURALYZER_GEOMETRYTargets.cmake
    NAMESPACE NEURALYZER_GEOMETRY::
    DESTINATION lib/cmake/NEURALYZER_GEOMETRY
)
