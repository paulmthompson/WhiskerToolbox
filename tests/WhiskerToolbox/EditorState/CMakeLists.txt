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

# EditorState unit tests - tests the core infrastructure only
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

# EditorState integration tests - tests cross-widget communication patterns
# These depend on concrete widget state implementations (MediaWidgetState, DataManagerWidgetState, DataTransformWidgetState)
add_executable(test_editor_state_integration
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/EditorState/EditorState.integration.test.cpp
)

target_link_libraries(test_editor_state_integration PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::EditorState
    DataManager
    DataManager_Widget
    Media_Widget
    DataTransform_Widget
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)

target_include_directories(test_editor_state_integration PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
)

set_target_properties(test_editor_state_integration PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

if (WIN32)
    catch_discover_tests(test_editor_state_integration
        DL_PATHS
        "$ENV{PATH}"
        "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_editor_state_integration)
endif()