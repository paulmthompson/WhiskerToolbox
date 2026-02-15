if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)
find_package(reflectcpp CONFIG REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

# Data struct tests (minimal dependencies — just reflect-cpp, no Qt runtime needed)
add_executable(test_state_management_data
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/StateManagement/AppPreferencesData.test.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/StateManagement/SessionData.test.cpp
)

target_link_libraries(test_state_management_data PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::StateManagement
)

if (WIN32)
    catch_discover_tests(test_state_management_data
        DL_PATHS
        "$ENV{PATH}"
        "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_state_management_data PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

# Qt integration tests (AppPreferences, SessionStore, StateManager — need QCoreApplication)
add_executable(test_state_management
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/StateManagement/StateManagement.test.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/StateManagement/WorkspaceManager.test.cpp
)

target_link_libraries(test_state_management PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::StateManagement
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)

# Enable Qt MOC for the test
set_target_properties(test_state_management PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

if (WIN32)
    catch_discover_tests(test_state_management
        DL_PATHS
        "$ENV{PATH}"
        "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_state_management PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()
