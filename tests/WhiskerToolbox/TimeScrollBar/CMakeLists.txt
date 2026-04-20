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

add_executable(test_frame_filter
    test_frame_filter.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/TimeScrollBar/FrameFilter.cpp
)

target_link_libraries(test_frame_filter PRIVATE
    Catch2::Catch2WithMain
    DataManager
    WhiskerToolbox::DigitalTimeSeries
)

target_include_directories(test_frame_filter PRIVATE
    "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox"
    "${CMAKE_SOURCE_DIR}/src"
)

if (ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_frame_filter)
else()
    add_test(NAME test_frame_filter_all
        COMMAND test_frame_filter
    )
endif()
