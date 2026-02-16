# StateManagement Library
# Provides application preferences, session memory, and workspace management.
# Phase 1: AppPreferences + SessionStore
# Phase 2: WorkspaceManager

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(reflectcpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(STATE_MANAGEMENT_SOURCES
    AppFileDialog.hpp
    AppFileDialog.cpp
    AppPreferencesData.hpp
    AppPreferences.hpp
    AppPreferences.cpp
    PathResolver.hpp
    SessionData.hpp
    SessionStore.hpp
    SessionStore.cpp
    WorkspaceData.hpp
    WorkspaceManager.hpp
    WorkspaceManager.cpp
    StateManager.hpp
    StateManager.cpp
)

add_library(StateManagement STATIC ${STATE_MANAGEMENT_SOURCES})

target_link_libraries(StateManagement
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        reflectcpp::reflectcpp
    PRIVATE
        nlohmann_json::nlohmann_json
        EditorState
        ZoneManager
)

target_include_directories(StateManagement
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add alias for consistent naming
add_library(WhiskerToolbox::StateManagement ALIAS StateManagement)
