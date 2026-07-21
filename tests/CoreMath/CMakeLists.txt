if (APPLE)
    message(STATUS "Testing Currenly not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currenly not supported on Windows")
    return()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_core_math
    non_finite_rows.test.cpp
)

target_link_libraries(test_core_math PRIVATE Catch2::Catch2WithMain WhiskerToolbox::CoreMath)

target_include_directories(test_core_math PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

catch_discover_tests(test_core_math)

set_target_compiler_warnings(test_core_math)

enable_whiskertoolbox_shared_test_pch(test_core_math)
