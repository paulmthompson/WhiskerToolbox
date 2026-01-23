cmake_minimum_required(VERSION 3.20)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# ==============================================================================
# Test 1: Basic ADS Docking Test (No ZoneManager)
# This tests raw Qt-Advanced-Docking-System behavior
# ==============================================================================
add_executable(BasicDockingTest
    BasicDockingTest.cpp
)

target_link_libraries(BasicDockingTest
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        qt6advanceddocking
)

set_target_properties(BasicDockingTest PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE FALSE  # Console app for debugging output
)

# ==============================================================================
# Test 2: ZoneManager Docking Test
# This tests docking behavior when using ZoneManager
# ==============================================================================
add_executable(ZoneManagerDockingTest
    ZoneManagerDockingTest.cpp
)

target_link_libraries(ZoneManagerDockingTest
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        qt6advanceddocking
        ZoneManager
)

target_include_directories(ZoneManagerDockingTest
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/ZoneManager
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/EditorState
)

set_target_properties(ZoneManagerDockingTest PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE FALSE  # Console app for debugging output
)

# ==============================================================================
# Test 3: Docking Debug Test
# Detailed debugging output for docking behavior diagnosis
# ==============================================================================
add_executable(DockingDebugTest
    DockingDebugTest.cpp
)

target_link_libraries(DockingDebugTest
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        qt6advanceddocking
)

set_target_properties(DockingDebugTest PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE FALSE  # Console app for debugging output
)

# ==============================================================================
# Test 4: Minimal Docking Test - No config flags at all
# ==============================================================================
add_executable(MinimalDockingTest
    MinimalDockingTest.cpp
)

target_link_libraries(MinimalDockingTest
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        qt6advanceddocking
)

set_target_properties(MinimalDockingTest PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE FALSE
)
