if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_timecontroller
    TimeController.test.cpp
)

target_include_directories(test_timecontroller PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

target_link_libraries(test_timecontroller PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::TimeController
)

if (WIN32)
    catch_discover_tests(test_timecontroller
        DL_PATHS
        "$ENV{PATH}"
        "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_timecontroller)
endif()

set_target_compiler_warnings(test_timecontroller)

enable_whiskertoolbox_shared_test_pch(test_timecontroller)
