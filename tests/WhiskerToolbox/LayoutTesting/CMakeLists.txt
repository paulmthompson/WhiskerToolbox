if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

# ── LayoutSanityChecker unit tests ────────────────────────────────────────────

add_executable(test_layout_sanity_checker
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/LayoutTesting/LayoutSanityChecker.test.cpp
)

set_target_properties(test_layout_sanity_checker PROPERTIES AUTOMOC ON)

target_link_libraries(test_layout_sanity_checker PRIVATE
    Catch2::Catch2WithMain
    LayoutTesting
    Qt6::Widgets
)

target_include_directories(test_layout_sanity_checker PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_layout_sanity_checker PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_layout_sanity_checker_all COMMAND test_layout_sanity_checker)
    set_property(TEST test_layout_sanity_checker_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_test_pch(test_layout_sanity_checker)

# ── ColormapControls layout sweep tests ───────────────────────────────────────

add_executable(test_colormap_controls_layout
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/LayoutTesting/ColormapControlsLayout.test.cpp
)

set_target_properties(test_colormap_controls_layout PROPERTIES AUTOMOC ON)

target_link_libraries(test_colormap_controls_layout PRIVATE
    Catch2::Catch2WithMain
    LayoutTesting
    ColormapControls
    Qt6::Widgets
)

target_include_directories(test_colormap_controls_layout PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/Common>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_colormap_controls_layout PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_colormap_controls_layout_all COMMAND test_colormap_controls_layout)
    set_property(TEST test_colormap_controls_layout_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_test_pch(test_colormap_controls_layout)

# ── Property panel layout sweep tests ─────────────────────────────────────────

add_executable(test_property_panel_layout
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/LayoutTesting/PropertyPanelLayout.test.cpp
)

set_target_properties(test_property_panel_layout PROPERTIES AUTOMOC ON)

target_link_libraries(test_property_panel_layout PRIVATE
    Catch2::Catch2WithMain
    LayoutTesting
    GlyphStyleWidget
    LineStyleControls
    RateEstimationControls
    Qt6::Widgets
)

target_include_directories(test_property_panel_layout PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/Common/GlyphStyleWidget>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/Common/LineStyleControls>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/Common/RateEstimationControls>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_property_panel_layout PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_property_panel_layout_all COMMAND test_property_panel_layout)
    set_property(TEST test_property_panel_layout_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_test_pch(test_property_panel_layout)

# ── HeatmapProperties layout integration tests ───────────────────────────────

add_executable(test_heatmap_properties_layout
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/LayoutTesting/HeatmapPropertiesLayout.test.cpp
)

set_target_properties(test_heatmap_properties_layout PROPERTIES AUTOMOC ON)

target_link_libraries(test_heatmap_properties_layout PRIVATE
    Catch2::Catch2WithMain
    LayoutTesting
    Collapsible_Widget
    ColormapControls
    RateEstimationControls
    Qt6::Widgets
)

target_include_directories(test_heatmap_properties_layout PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/Common>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_heatmap_properties_layout PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_heatmap_properties_layout_all COMMAND test_heatmap_properties_layout)
    set_property(TEST test_heatmap_properties_layout_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_test_pch(test_heatmap_properties_layout)