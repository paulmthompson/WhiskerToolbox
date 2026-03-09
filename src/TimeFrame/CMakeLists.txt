# TimeFrame Static Library
# This library provides time handling and strong time types implementation

add_library(TimeFrame STATIC
    TimeFrame.hpp
    TimeFrame.cpp
    TimeFrameIndex.hpp
    StrongTimeTypes.hpp
    TimeIndexStorage.hpp
    TimeIndexStorage.cpp
    interval_data.hpp
)

# Set up include directories
target_include_directories(TimeFrame PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Apply the same compiler flags as the main project
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(TimeFrame PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(TimeFrame PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(TimeFrame PRIVATE ${MSVC_WARNINGS})
endif()

# Set target properties to match the main project
set_target_properties(TimeFrame PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add alias for consistent naming
add_library(WhiskerToolbox::TimeFrame ALIAS TimeFrame)

enable_whiskertoolbox_pch(TimeFrame)
