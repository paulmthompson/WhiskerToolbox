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

get_property(DATAVIEWER_TEST_SOURCES_TEMP GLOBAL PROPERTY DATAVIEWER_TEST_SOURCES)
message(STATUS "DATAVIEWER_TEST_SOURCES_TEMP: ${DATAVIEWER_TEST_SOURCES_TEMP}")

add_executable(test_data_viewer

        test_data_viewer.cpp

        ${DATAVIEWER_TEST_SOURCES_TEMP}
)

target_link_libraries(test_data_viewer PRIVATE Catch2::Catch2WithMain DataViewer DataManager CorePlotting)

target_include_directories(test_data_viewer PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataViewer>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager>"
)

if (APPLE)
    set_target_properties(test_data_viewer PROPERTIES
            BUILD_RPATH "${CMAKE_BINARY_DIR};${CMAKE_BINARY_DIR}/_deps/catch2-build/src"
            INSTALL_RPATH "@executable_path/../Frameworks"
    )
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

endif()

if(ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_data_viewer
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_data_viewer)
    endif()
else()
    add_test(NAME test_data_viewer_all
        COMMAND test_data_viewer
        )
endif()