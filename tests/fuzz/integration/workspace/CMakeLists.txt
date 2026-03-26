# EditorRegistry workspace serialization fuzz tests
# Phase 3 of Workspace Fuzz Testing Roadmap:
# Registry toJson()/fromJson() round-trip and crash-resistance testing

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Test)
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
add_fuzz_test(fuzz_registry
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

add_fuzz_test(fuzz_workspace_placement
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;QT_QPA_PLATFORM=offscreen"
)

# ==============================================================================
# Phase 5: Workspace File Parsing Fuzz Tests
# WorkspaceManager::readWorkspace() with arbitrary/malformed .wtb files
# ==============================================================================

add_executable(fuzz_workspace_file
    MockEditorTypes.cpp
    fuzz_workspace_file.cpp
)

add_fuzz_test_with_instantiations(fuzz_workspace_file)

set_target_properties(fuzz_workspace_file PROPERTIES
    AUTOMOC ON
)

target_include_directories(fuzz_workspace_file
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(fuzz_workspace_file
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
        # StateManagement (provides WorkspaceManager)
        WhiskerToolbox::StateManagement
)

target_compile_options(fuzz_workspace_file
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_workspace_file
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_workspace_file
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;QT_QPA_PLATFORM=offscreen"
)

# ==============================================================================
# Phase 6: Real Widget Adoption Fuzz Tests
# Real EditorState subclasses (starting with TestWidget) through the full
# placement → serialize → restore → verify pipeline.
# ==============================================================================

add_executable(fuzz_real_widget_roundtrip
    FuzzDockingHarness.cpp
    MockEditorTypes.cpp
    WidgetCorpus.cpp
    fuzz_real_widget_roundtrip.cpp
    # State-only sources for Core level widgets (avoids linking full widget UI libraries)
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataInspector_Widget/DataInspectorState.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataImport_Widget/DataImportWidgetState.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataTransform_Widget/DataTransformWidgetState.cpp
    # State-only sources for Visualization level widgets
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataViewer_Widget/Core/DataViewerState.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataViewer_Widget/Core/SeriesOptionsRegistry.cpp
    # State-only sources for Media level widgets
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Media_Widget/Core/MediaWidgetState.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Media_Widget/DisplayOptionsRegistry.cpp
)

add_fuzz_test_with_instantiations(fuzz_real_widget_roundtrip)

set_target_properties(fuzz_real_widget_roundtrip PROPERTIES
    AUTOMOC ON
)

target_include_directories(fuzz_real_widget_roundtrip
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
        # Media_Widget state-only compilation needs this for DisplayOptions/DisplayOptions.hpp
        # and its relative include "../../DataManager/utils/color.hpp"
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Media_Widget
)

target_link_libraries(fuzz_real_widget_roundtrip
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
        Qt6::Gui
        Qt6::Test
        # Editor framework
        WhiskerToolbox::EditorState
        # Zone/docking
        ZoneManager
        WhiskerToolbox::StateManagement
        qt6advanceddocking
        # DataManager (needed by real widget state constructors)
        DataManager
        # CorePlotting (needed by DataViewerStateData — TimeSeriesViewState, SeriesStyle)
        CorePlotting
        # TensorData (transitive dep: CorePlotting → TransformsV2 → TensorData)
        WhiskerToolbox::TensorData
        # Real widget libraries (Phase 6 corpus)
        Test_Widget

        CoreUtilities # for color in MediaWidget state

        ImageProcessing # for Display Options in Media State
)

target_compile_options(fuzz_real_widget_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_real_widget_roundtrip
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_real_widget_roundtrip
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;QT_QPA_PLATFORM=offscreen"
)
