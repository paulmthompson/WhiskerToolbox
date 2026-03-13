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

add_executable(test_line_plot_widget
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/Plots/LinePlotWidget/LinePlotPropertiesWidget.test.cpp
)

target_link_libraries(test_line_plot_widget PRIVATE 
    Catch2::Catch2WithMain 
    LinePlotWidget 
    PlotAlignmentWidget
    DataManager 
    EditorState
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    WhiskerToolbox::LineData
)

target_include_directories(test_line_plot_widget PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/LinePlotWidget>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/PlotAlignmentWidget>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

# Enable Qt MOC for the test
set_target_properties(test_line_plot_widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_line_plot_widget PROPERTIES 
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_line_plot_widget_all
        COMMAND test_line_plot_widget
        )
    set_property(TEST test_line_plot_widget_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_line_plot_widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_line_plot_widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_line_plot_widget PRIVATE ${MSVC_WARNINGS})
endif()

enable_whiskertoolbox_shared_test_pch(test_line_plot_widget)