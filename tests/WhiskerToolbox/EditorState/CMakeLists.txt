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