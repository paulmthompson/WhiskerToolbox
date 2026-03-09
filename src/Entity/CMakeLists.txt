# Entity Static Library
# This library provides entity management, registration, and lineage tracking functionality

add_library(Entity STATIC
    # Core types
    EntityTypes.hpp
    EntityRegistry.hpp
    EntityRegistry.cpp
    EntityGroupManager.hpp
    EntityGroupManager.cpp
    
    # Lineage subsystem (moved from DataManager in Phase 2)
    Lineage/LineageTypes.hpp
    Lineage/LineageRegistry.hpp
    Lineage/LineageRegistry.cpp
    # Abstract resolver interface and implementation (Phase 3)
    Lineage/LineageResolver.hpp
    Lineage/LineageResolver.cpp
)

# Set up include directories
target_include_directories(Entity PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>" # For TimeFrame includes
)

# Link to TimeFrame library since EntityRegistry depends on TimeFrameIndex
# Link to ObserverData since EntityGroupManager uses observer pattern
target_link_libraries(Entity PUBLIC 
    WhiskerToolbox::TimeFrame
    WhiskerToolbox::ObserverData
)

# Apply the same compiler flags as the main project
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Entity PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Entity PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(Entity PRIVATE ${MSVC_WARNINGS})
endif()

# Set target properties to match the main project
set_target_properties(Entity PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add alias for consistent naming
add_library(WhiskerToolbox::Entity ALIAS Entity)
