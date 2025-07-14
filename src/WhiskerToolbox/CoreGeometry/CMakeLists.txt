# NEURALYZER_GEOMETRY
# This library provides header-only geometry utilities

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
