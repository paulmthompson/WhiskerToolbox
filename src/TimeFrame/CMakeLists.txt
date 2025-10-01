# TimeFrame Shared Library
# This library provides time handling and strong time types implementation

add_library(TimeFrame SHARED
    TimeFrame.hpp
    TimeFrame.cpp
    StrongTimeTypes.hpp
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
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    VERSION 1.0.0
    SOVERSION 1
)

# Add alias for consistent naming
add_library(WhiskerToolbox::TimeFrame ALIAS TimeFrame)

# Export symbols for shared library
if(WIN32)
    set_target_properties(TimeFrame PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

# Add compiler definitions for shared library
target_compile_definitions(TimeFrame PRIVATE TIMEFRAME_BUILDING_DLL)
target_compile_definitions(TimeFrame INTERFACE TIMEFRAME_USING_DLL)
