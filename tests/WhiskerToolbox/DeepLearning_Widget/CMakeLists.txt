if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Test)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_dl_widget_helpers
    DataSourceComboHelper.test.cpp
)

target_link_libraries(test_dl_widget_helpers PRIVATE
    Catch2::Catch2WithMain
    DeepLearning_Widget
    DataManager
    WhiskerToolbox::PointData
    WhiskerToolbox::LineData
    WhiskerToolbox::MaskData
    WhiskerToolbox::MediaData
    Qt6::Widgets
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_dl_widget_helpers PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_dl_widget_helpers PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_dl_widget_helpers PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_dl_widget_helpers PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_dl_widget_helpers
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_dl_widget_helpers_all
        COMMAND test_dl_widget_helpers
    )
    set_property(TEST test_dl_widget_helpers_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_widget_helpers)

# ── DynamicInputSlotWidget tests ────────────────────────────────────────────

add_executable(test_dl_dynamic_input_slot
    DynamicInputSlotWidget.test.cpp
)

target_link_libraries(test_dl_dynamic_input_slot PRIVATE
    Catch2::Catch2WithMain
    DeepLearning_Widget
    DataManager
    WhiskerToolbox::PointData
    WhiskerToolbox::LineData
    WhiskerToolbox::MaskData
    WhiskerToolbox::MediaData
    Qt6::Widgets
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_dl_dynamic_input_slot PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_dl_dynamic_input_slot PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_dl_dynamic_input_slot PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_dl_dynamic_input_slot PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_dl_dynamic_input_slot
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_dl_dynamic_input_slot_all
        COMMAND test_dl_dynamic_input_slot
    )
    set_property(TEST test_dl_dynamic_input_slot_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_dynamic_input_slot)

# ── StaticInputSlotWidget tests ─────────────────────────────────────────────

add_executable(test_dl_static_input_slot
    StaticInputSlotWidget.test.cpp
)

target_link_libraries(test_dl_static_input_slot PRIVATE
    Catch2::Catch2WithMain
    DeepLearning_Widget
    DataManager
    WhiskerToolbox::PointData
    WhiskerToolbox::LineData
    WhiskerToolbox::MaskData
    WhiskerToolbox::MediaData
    Qt6::Widgets
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_dl_static_input_slot PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_dl_static_input_slot PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_dl_static_input_slot PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_dl_static_input_slot PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_dl_static_input_slot
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_dl_static_input_slot_all
        COMMAND test_dl_static_input_slot
    )
    set_property(TEST test_dl_static_input_slot_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_static_input_slot)
