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

# Create TimeFrame test executable
add_executable(test_timeframe
        test_timeframe_main.cpp
        ${CMAKE_SOURCE_DIR}/src/TimeFrame/StrongTimeTypes.test.cpp
)

target_link_libraries(test_timeframe PRIVATE 
    Catch2::Catch2WithMain 
    WhiskerToolbox::TimeFrame
)

target_include_directories(test_timeframe PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/TimeFrame>"
)

if (APPLE)
    set_target_properties(test_timeframe PROPERTIES
            BUILD_RPATH "${CMAKE_BINARY_DIR};${CMAKE_BINARY_DIR}/_deps/catch2-build/src"
            INSTALL_RPATH "@executable_path/../Frameworks"
    )
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

if (WIN32)
    catch_discover_tests(test_timeframe
            DL_PATHS
            "$ENV{PATH}"
            "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_timeframe)
endif()
