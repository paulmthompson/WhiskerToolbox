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

set_target_compiler_warnings(TriageSession)

# Set target properties to match the main project
set_target_properties(TriageSession PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Dependencies: TriageSession consumes the Command Architecture from DataManager
target_link_libraries(TriageSession PUBLIC DataManager Commands)

# Add alias for consistent naming
add_library(WhiskerToolbox::TriageSession ALIAS TriageSession)
enable_whiskertoolbox_shared_pch(TriageSession)