if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_plot_alignment_gather
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/Plots/Common/PlotAlignmentGather.test.cpp
)

target_link_libraries(test_plot_alignment_gather PRIVATE 
    Catch2::Catch2WithMain 
    DataManager
    PlotAlignmentWidget
    Qt6::Core
    GatherResult
)

target_include_directories(test_plot_alignment_gather PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/Common>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_plot_alignment_gather PROPERTIES 
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_plot_alignment_gather COMMAND test_plot_alignment_gather)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_plot_alignment_gather PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_plot_alignment_gather PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_plot_alignment_gather PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_test_pch(test_plot_alignment_gather)

# ── PlotInteractionHelpers tests ──────────────────────────────────────────────

find_package(glm CONFIG REQUIRED)

add_executable(test_plot_interaction_helpers
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/Plots/Common/PlotInteractionHelpers.test.cpp
)

target_link_libraries(test_plot_interaction_helpers PRIVATE
    Catch2::Catch2WithMain
    Qt6::Core
    glm::glm
)

target_include_directories(test_plot_interaction_helpers PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/Common>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_plot_interaction_helpers PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_plot_interaction_helpers COMMAND test_plot_interaction_helpers)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_plot_interaction_helpers PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_plot_interaction_helpers PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_plot_interaction_helpers PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_test_pch(test_plot_interaction_helpers)

# ── Common DataManager integration tests (template-based) ────────────────────

# ── EventRateEstimation tests ─────────────────────────────────────────────────

add_executable(test_event_rate_estimation
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/Plots/Common/EventRateEstimation.test.cpp
)

target_link_libraries(test_event_rate_estimation PRIVATE
    Catch2::Catch2WithMain
    EventRateEstimation
    DataManager
)

target_include_directories(test_event_rate_estimation PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/Common>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_event_rate_estimation PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_event_rate_estimation_all
        COMMAND test_event_rate_estimation
    )
    set_property(TEST test_event_rate_estimation_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_event_rate_estimation PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_event_rate_estimation PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_event_rate_estimation PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_test_pch(test_event_rate_estimation)

# ── Common DataManager integration tests (template-based) ────────────────────

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Test)

add_executable(test_plot_widget_common
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/Plots/Common/PlotWidgetCommonTests.test.cpp
)

target_link_libraries(test_plot_widget_common PRIVATE
    Catch2::Catch2WithMain
    LinePlotWidget
    EventPlotWidget
    ACFWidget
    PlotAlignmentWidget
    DataManager
    EditorState
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

target_include_directories(test_plot_widget_common PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/Common>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/Plots/Common>"
)

set_target_properties(test_plot_widget_common PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_plot_widget_common PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_plot_widget_common_all
        COMMAND test_plot_widget_common
    )
    set_property(TEST test_plot_widget_common_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_plot_widget_common PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_plot_widget_common PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_plot_widget_common PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_test_pch(test_plot_widget_common)