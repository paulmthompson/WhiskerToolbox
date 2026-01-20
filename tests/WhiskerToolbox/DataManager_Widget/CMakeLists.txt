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

add_executable(test_data_manager_widget
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager_Widget/DataManager_Widget.test.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager_Widget/DataManagerWidgetState.test.cpp
)

target_link_libraries(test_data_manager_widget PRIVATE 
    Catch2::Catch2WithMain 
    DataManager_Widget 
    DataManager 
    Qt6::Widgets
    Qt6::Core
    Qt6::Test
    TimeScrollBar
)

target_include_directories(test_data_manager_widget PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager_Widget>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/DataManager_Widget>"
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_data_manager_widget)
else()
    add_test(NAME test_data_manager_widget_all
        COMMAND test_data_manager_widget
        )
endif()