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

# TriageSession tests use the co-located test file from src/TriageSession
add_executable(test_triage_session
    ${CMAKE_SOURCE_DIR}/src/TriageSession/TriageSession.test.cpp
)

target_link_libraries(test_triage_session PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::TriageSession
)

if (WIN32)
    catch_discover_tests(test_triage_session
        DL_PATHS
        "$ENV{PATH}"
        "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_triage_session)
endif()
enable_whiskertoolbox_test_pch(test_triage_session)