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

get_property(CORE_GEOMETRY_TEST_SOURCES_TEMP GLOBAL PROPERTY CORE_GEOMETRY_TEST_SOURCES)
message(STATUS "CORE_GEOMETRY_TEST_SOURCES_TEMP: ${CORE_GEOMETRY_TEST_SOURCES_TEMP}")

add_executable(test_core_geometry

        ${CORE_GEOMETRY_TEST_SOURCES_TEMP}
)

target_link_libraries(test_core_geometry PRIVATE Catch2::Catch2WithMain NEURALYZER_GEOMETRY)

target_include_directories(test_core_geometry PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/CoreGeometry/include>"
)

if (APPLE)
    set_target_properties(test_core_geometry PROPERTIES
            BUILD_RPATH "${CMAKE_BINARY_DIR};${CMAKE_BINARY_DIR}/_deps/catch2-build/src"
            INSTALL_RPATH "@executable_path/../Frameworks"
    )
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

endif()

if (WIN32)
    catch_discover_tests(test_core_geometry
            DL_PATHS
            "$ENV{PATH}"
            "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_core_geometry)
endif()
