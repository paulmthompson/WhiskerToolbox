# EditorRegistry workspace serialization fuzz tests
# Phase 3 of Workspace Fuzz Testing Roadmap:
# Registry toJson()/fromJson() round-trip and crash-resistance testing

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)
find_package(reflectcpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(fuzz_registry
    MockEditorTypes.cpp
    fuzz_registry_roundtrip.cpp
    fuzz_registry_from_json.cpp
)

add_fuzz_test_with_instantiations(fuzz_registry)

# Enable Qt MOC for EditorState and mock QObject subclasses
set_target_properties(fuzz_registry PROPERTIES
    AUTOMOC ON
)

target_include_directories(fuzz_registry
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link EditorRegistry (includes EditorState, SelectionContext) + mock types + Qt
target_link_libraries(fuzz_registry
    PRIVATE
        # Fuzz/test infrastructure
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
        nlohmann_json::nlohmann_json
        reflectcpp::reflectcpp
        # Qt
        Qt6::Core
        Qt6::Widgets
        Qt6::Test
        # EditorState library (includes EditorRegistry, SelectionContext, etc.)
        WhiskerToolbox::EditorState
)

# Add AddressSanitizer for better bug detection
target_compile_options(fuzz_registry
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_registry
    PRIVATE
        -fsanitize=address
)

# Suppress LeakSanitizer (yyjson leaks) and use offscreen Qt platform
gtest_discover_tests(fuzz_registry
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;QT_QPA_PLATFORM=offscreen"
)

# ==============================================================================
# Phase 4: Zone Placement Fuzz Tests
# EditorCreationController + ZoneManager widget placement → serialize → restore
# ==============================================================================

add_executable(fuzz_workspace_placement
    FuzzDockingHarness.cpp
    MockEditorTypes.cpp
    fuzz_workspace_placement.cpp
)

add_fuzz_test_with_instantiations(fuzz_workspace_placement)

set_target_properties(fuzz_workspace_placement PROPERTIES
    AUTOMOC ON
)

target_include_directories(fuzz_workspace_placement
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(fuzz_workspace_placement
    PRIVATE
        # Fuzz/test infrastructure
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
        nlohmann_json::nlohmann_json
        reflectcpp::reflectcpp
        # Qt
        Qt6::Core
        Qt6::Widgets
        Qt6::Test
        # Editor framework
        WhiskerToolbox::EditorState
        # Zone/docking (Option B: link both ZoneManager + StateManagement)
        ZoneManager
        WhiskerToolbox::StateManagement
        qt6advanceddocking
)

target_compile_options(fuzz_workspace_placement
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_workspace_placement
    PRIVATE
        -fsanitize=address
)

gtest_discover_tests(fuzz_workspace_placement
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;QT_QPA_PLATFORM=offscreen"
)
