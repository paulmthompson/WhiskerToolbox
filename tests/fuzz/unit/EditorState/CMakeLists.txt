# EditorState data struct fuzz tests
# Phase 1 of Workspace Fuzz Testing Roadmap:
# Pure data round-trip and crash-resistance testing for StateData structs

find_package(reflectcpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(fuzz_state_data
    fuzz_state_data_roundtrip.cpp
    fuzz_state_data_from_garbage.cpp
)

add_fuzz_test_with_instantiations(fuzz_state_data)

# Include paths: src/WhiskerToolbox for widget StateData headers,
# src/ for CorePlotting headers
target_include_directories(fuzz_state_data
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox
        ${CMAKE_SOURCE_DIR}/src
)

# Link only what's needed for pure data struct serialization — NO Qt required
target_link_libraries(fuzz_state_data
    PRIVATE
        reflectcpp::reflectcpp
        nlohmann_json::nlohmann_json
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
)

# Add AddressSanitizer for better bug detection
# Note: Do NOT use -fsanitize=fuzzer here - FuzzTest uses GoogleTest mode
target_compile_options(fuzz_state_data
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_state_data
    PRIVATE
        -fsanitize=address
)

# Register with gtest discovery
# Suppress LeakSanitizer — yyjson (reflect-cpp's JSON backend) has benign leaks
# in yyjson_mut_doc_new that are outside our control.
gtest_discover_tests(fuzz_state_data
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)

# =============================================================================
# EditorState object fuzz tests
# Phase 2 of Workspace Fuzz Testing Roadmap:
# Full EditorState::toJson()/fromJson() round-trip and crash-resistance testing
# =============================================================================

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)

add_executable(fuzz_editor_state
    fuzz_editor_state_roundtrip.cpp
    fuzz_editor_state_from_json.cpp
)

add_fuzz_test_with_instantiations(fuzz_editor_state)

# Enable Qt MOC for EditorState QObject subclasses
set_target_properties(fuzz_editor_state PROPERTIES
    AUTOMOC ON
)

target_include_directories(fuzz_editor_state
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox
        ${CMAKE_SOURCE_DIR}/src
)

# Link EditorState base + concrete widget state libraries + Qt
target_link_libraries(fuzz_editor_state
    PRIVATE
        # Fuzz/test infrastructure
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
        nlohmann_json::nlohmann_json
        # Qt
        Qt6::Core
        Qt6::Widgets
        Qt6::Test
        # EditorState base class
        WhiskerToolbox::EditorState
        # DataManager (needed by widget state constructors)
        DataManager
        # Concrete widget state libraries (9 types)
        DataManager_Widget
        TimeScrollBar
        DataTransform_Widget
        GroupManagementWidget
        DataImport_Widget
        DataInspector_Widget
        LinePlotWidget
        EventPlotWidget
        Media_Widget
)

target_compile_options(fuzz_editor_state
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_editor_state
    PRIVATE
        -fsanitize=address
)

# Suppress LeakSanitizer (yyjson leaks) and use offscreen Qt platform
gtest_discover_tests(fuzz_editor_state
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;QT_QPA_PLATFORM=offscreen"
)
