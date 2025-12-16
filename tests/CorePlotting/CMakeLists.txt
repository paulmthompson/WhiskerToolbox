if (APPLE)
    message(STATUS "CorePlotting tests currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "CorePlotting tests currently not supported on Windows")
    return()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

find_package(glm CONFIG REQUIRED)

# Create CorePlotting test executable
add_executable(test_coreplotting
    TimeRange.test.cpp
    TimeAxisCoordinates.test.cpp
    SeriesCoordinateQuery.test.cpp
    SeriesMatrices.test.cpp
    LayoutEngine.test.cpp
    IntegrationTests.test.cpp
    TimeFrameAdapters.test.cpp
    HitTestResult.test.cpp
    SceneHitTester.test.cpp
    IntervalDragController.test.cpp
)

target_link_libraries(test_coreplotting PRIVATE 
    Catch2::Catch2WithMain
    CorePlotting
    DataManager
    WhiskerToolbox::TimeFrame
    NEURALYZER_GEOMETRY
    glm::glm
)

target_include_directories(test_coreplotting PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CorePlotting>"
)

if (APPLE)
    add_custom_command(
            TARGET test_coreplotting POST_BUILD
            COMMAND install_name_tool -add_rpath "@executable_path/../lib" "$<TARGET_FILE:test_coreplotting>"
    )
endif ()

# Register with CTest
catch_discover_tests(test_coreplotting)
