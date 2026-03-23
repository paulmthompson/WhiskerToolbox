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

# ── OutputSlotWidget tests ───────────────────────────────────────────────────

add_executable(test_dl_output_slot
    OutputSlotWidget.test.cpp
)

target_link_libraries(test_dl_output_slot PRIVATE
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

target_include_directories(test_dl_output_slot PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_dl_output_slot PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_dl_output_slot PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_dl_output_slot PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_dl_output_slot
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_dl_output_slot_all
        COMMAND test_dl_output_slot
    )
    set_property(TEST test_dl_output_slot_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_output_slot)

# ── PostEncoderWidget tests ──────────────────────────────────────────────

add_executable(test_dl_post_encoder
    PostEncoderWidget.test.cpp
)

target_link_libraries(test_dl_post_encoder PRIVATE
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

target_include_directories(test_dl_post_encoder PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_dl_post_encoder PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_dl_post_encoder PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_dl_post_encoder PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_dl_post_encoder
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_dl_post_encoder_all
        COMMAND test_dl_post_encoder
    )
    set_property(TEST test_dl_post_encoder_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_post_encoder)

# ── EncoderShapeWidget tests ──────────────────────────────────────────────

add_executable(test_dl_encoder_shape
    EncoderShapeWidget.test.cpp
)

target_link_libraries(test_dl_encoder_shape PRIVATE
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

target_include_directories(test_dl_encoder_shape PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_dl_encoder_shape PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_dl_encoder_shape PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_dl_encoder_shape PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_dl_encoder_shape
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_dl_encoder_shape_all
        COMMAND test_dl_encoder_shape
    )
    set_property(TEST test_dl_encoder_shape_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_encoder_shape)

# ── RecurrentBindingWidget tests ───────────────────────────────────────────

add_executable(test_dl_recurrent_binding
    RecurrentBindingWidget.test.cpp
)

target_link_libraries(test_dl_recurrent_binding PRIVATE
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

target_include_directories(test_dl_recurrent_binding PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_dl_recurrent_binding PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_dl_recurrent_binding PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_dl_recurrent_binding PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_dl_recurrent_binding
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_dl_recurrent_binding_all
        COMMAND test_dl_recurrent_binding
    )
    set_property(TEST test_dl_recurrent_binding_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_recurrent_binding)

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

# ── SequenceSlotWidget tests ────────────────────────────────────────────────

add_executable(test_dl_sequence_slot
    SequenceSlotWidget.test.cpp
)

target_link_libraries(test_dl_sequence_slot PRIVATE
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

target_include_directories(test_dl_sequence_slot PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_dl_sequence_slot PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_dl_sequence_slot PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_dl_sequence_slot PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_dl_sequence_slot
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_dl_sequence_slot_all
        COMMAND test_dl_sequence_slot
    )
    set_property(TEST test_dl_sequence_slot_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_sequence_slot)

# ── ConstraintEnforcer tests ─────────────────────────────────────────────────

add_executable(test_dl_constraint_enforcer
    ConstraintEnforcer.test.cpp
)

target_link_libraries(test_dl_constraint_enforcer PRIVATE
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

target_include_directories(test_dl_constraint_enforcer PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_dl_constraint_enforcer PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_dl_constraint_enforcer PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_dl_constraint_enforcer PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_dl_constraint_enforcer
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_dl_constraint_enforcer_all
        COMMAND test_dl_constraint_enforcer
    )
    set_property(TEST test_dl_constraint_enforcer_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_constraint_enforcer)
