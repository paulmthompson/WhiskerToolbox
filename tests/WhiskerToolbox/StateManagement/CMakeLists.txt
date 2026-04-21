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
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/StateManagement/PathResolver.test.cpp
)

target_link_libraries(test_state_management_data PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::StateManagement
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_state_management_data PROPERTIES 
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_state_management_data_all
        COMMAND test_state_management_data
        )
    set_property(TEST test_state_management_data_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

set_target_compiler_warnings(test_state_management_data)

enable_whiskertoolbox_shared_test_pch(test_state_management_data)

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

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_state_management PROPERTIES 
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_state_management_all
        COMMAND test_state_management
        )
    set_property(TEST test_state_management_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

set_target_compiler_warnings(test_state_management)

enable_whiskertoolbox_shared_test_pch(test_state_management)