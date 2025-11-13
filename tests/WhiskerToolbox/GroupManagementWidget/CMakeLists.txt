if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Test)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_group_management_widget
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/GroupManagementWidget/GroupManager.test.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/GroupManagementWidget/GroupManagementWidget.test.cpp
)

target_link_libraries(test_group_management_widget PRIVATE 
    Catch2::Catch2WithMain 
    GroupManagementWidget 
    DataManager 
    Qt6::Widgets
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_group_management_widget PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/GroupManagementWidget>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
)

# Enable Qt MOC for the test
set_target_properties(test_group_management_widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_group_management_widget PROPERTIES 
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_group_management_widget_all
        COMMAND test_group_management_widget
        )
    set_property(TEST test_group_management_widget_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()