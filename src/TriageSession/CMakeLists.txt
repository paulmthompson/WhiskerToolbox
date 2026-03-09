# TriageSession Static Library
# Mark/Commit/Recall state machine consuming command sequences from the
# Command Architecture. No Qt dependency.

add_library(TriageSession STATIC
    TriageSession.hpp
    TriageSession.cpp
)

# Set up include directories
target_include_directories(TriageSession PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"  # For "TriageSession/..." includes
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Apply the same compiler flags as the main project
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(TriageSession PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(TriageSession PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(TriageSession PRIVATE ${MSVC_WARNINGS})
endif()

# Set target properties to match the main project
set_target_properties(TriageSession PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Dependencies: TriageSession consumes the Command Architecture from DataManager
target_link_libraries(TriageSession PUBLIC DataManager)

# Add alias for consistent naming
add_library(WhiskerToolbox::TriageSession ALIAS TriageSession)
enable_whiskertoolbox_pch(TriageSession)