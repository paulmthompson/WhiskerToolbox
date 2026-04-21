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
    ${CMAKE_SOURCE_DIR}/tests/shared/RegisterCommandsListener.cpp
    ${CMAKE_SOURCE_DIR}/src/TriageSession/TriageSession.test.cpp
)

target_link_libraries(test_triage_session PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::TriageSession
)

target_link_libraries(test_triage_session PRIVATE WhiskerToolbox::TimeController)
target_link_libraries(test_triage_session PRIVATE DataManager)
target_link_libraries(test_triage_session PRIVATE Commands)
target_link_libraries(test_triage_session PRIVATE WhiskerToolbox::LineData)
target_link_libraries(test_triage_session PRIVATE WhiskerToolbox::DigitalTimeSeries)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_triage_session)
else()
    add_test(NAME test_triage_session_all
        COMMAND test_triage_session
    )
endif()

set_target_compiler_warnings(test_triage_session)

enable_whiskertoolbox_shared_test_pch(test_triage_session)