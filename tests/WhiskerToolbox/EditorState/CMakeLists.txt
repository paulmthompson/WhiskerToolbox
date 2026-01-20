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

# EditorState tests use the co-located test file from src/WhiskerToolbox/EditorState
add_executable(test_editor_state
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/EditorState/EditorState.test.cpp
)

target_link_libraries(test_editor_state PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::EditorState
    Qt6::Core
    Qt6::Test
)

# Enable Qt MOC for the test
set_target_properties(test_editor_state PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

if (WIN32)
    catch_discover_tests(test_editor_state
        DL_PATHS
        "$ENV{PATH}"
        "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_editor_state)
endif()

# DataManagerWidgetState tests
add_executable(test_data_manager_widget_state
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager_Widget/DataManagerWidgetState.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager_Widget/DataManagerWidgetState.test.cpp
)

target_link_libraries(test_data_manager_widget_state PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::EditorState
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_data_manager_widget_state PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager_Widget>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
)

set_target_properties(test_data_manager_widget_state PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

catch_discover_tests(test_data_manager_widget_state)

# MediaWidgetState tests
add_executable(test_media_widget_state
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Media_Widget/MediaWidgetState.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Media_Widget/MediaWidgetState.test.cpp
)

target_link_libraries(test_media_widget_state PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::EditorState
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_media_widget_state PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Media_Widget>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
)

set_target_properties(test_media_widget_state PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

catch_discover_tests(test_media_widget_state)
