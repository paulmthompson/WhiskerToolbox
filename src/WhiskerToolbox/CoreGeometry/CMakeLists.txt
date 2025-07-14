# NEURALYZER_GEOMETRY
# This library provides header-only geometry utilities

set(NEURALYZER_GEOMETRY_INCLUDES
    include/CoreGeometry/boundingbox.hpp
    include/CoreGeometry/geometry.hpp
    include/CoreGeometry/ImageSize.hpp
    include/CoreGeometry/masks.hpp
    include/CoreGeometry/points.hpp
)

set(NEURALYZER_GEOMETRY_SOURCES
   src/masks.cpp
   src/points.cpp
)

set_property(GLOBAL PROPERTY CORE_GEOMETRY_TEST_SOURCES 
    src/masks.test.cpp
)

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
