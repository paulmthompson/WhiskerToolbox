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

add_executable(test_auto_param_widget
    AutoParamWidget.test.cpp
)

target_link_libraries(test_auto_param_widget PRIVATE
    Catch2::Catch2WithMain
    AutoParamWidget
    WhiskerToolbox::ParameterSchema
    Qt6::Widgets
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_auto_param_widget PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_auto_param_widget PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_auto_param_widget PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_auto_param_widget PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_auto_param_widget
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_auto_param_widget_all
        COMMAND test_auto_param_widget
    )
    set_property(TEST test_auto_param_widget_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()
