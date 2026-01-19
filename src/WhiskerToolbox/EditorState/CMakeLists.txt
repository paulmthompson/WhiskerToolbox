# EditorState Library
# Provides the core infrastructure for widget state management,
# inter-widget communication, and workspace serialization.

find_package(reflectcpp CONFIG REQUIRED)

set(EDITOR_STATE_SOURCES
    EditorState.cpp
    EditorState.hpp
    SelectionContext.cpp
    SelectionContext.hpp
    WorkspaceManager.cpp
    WorkspaceManager.hpp
)

add_library(EditorState STATIC ${EDITOR_STATE_SOURCES})

target_link_libraries(EditorState
    PUBLIC
        Qt6::Core
        reflectcpp::reflectcpp
)

target_include_directories(EditorState
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Add alias for consistent naming
add_library(WhiskerToolbox::EditorState ALIAS EditorState)
