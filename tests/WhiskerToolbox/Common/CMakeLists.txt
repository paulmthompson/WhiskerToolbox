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

add_executable(test_scrollable_combo_box
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/Common/ScrollableComboBox.test.cpp
)

target_link_libraries(test_scrollable_combo_box PRIVATE
    Catch2::Catch2WithMain
    ScrollableComboBox
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

target_include_directories(test_scrollable_combo_box PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
)

set_target_properties(test_scrollable_combo_box PROPERTIES
    AUTOMOC ON
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_scrollable_combo_box PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_scrollable_combo_box_all
        COMMAND test_scrollable_combo_box
    )
    set_property(TEST test_scrollable_combo_box_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

set_target_compiler_warnings(test_scrollable_combo_box)

enable_whiskertoolbox_shared_test_pch(test_scrollable_combo_box)
