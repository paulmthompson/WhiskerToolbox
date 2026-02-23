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

add_executable(test_transforms_v2_widget
    TransformsV2Widget.test.cpp
)

target_link_libraries(test_transforms_v2_widget PRIVATE
    Catch2::Catch2WithMain
    TransformsV2_Widget
    DataManager
    EditorState
    WhiskerToolbox::TensorData
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

# Force whole-archive linking for TransformsV2 to ensure static transform registration
# (RegisteredTransforms.cpp has file-scope RAII objects that register transforms into
# ElementRegistry, but the linker would strip them since tests have no direct symbol references)
if(APPLE)
    target_link_libraries(test_transforms_v2_widget PRIVATE -Wl,-force_load,$<TARGET_FILE:TransformsV2>)
elseif(MSVC)
    target_link_options(test_transforms_v2_widget PRIVATE /WHOLEARCHIVE:TransformsV2)
    target_link_libraries(test_transforms_v2_widget PRIVATE TransformsV2)
else()
    # Linux/GCC/Clang
    target_link_libraries(test_transforms_v2_widget PRIVATE -Wl,--whole-archive TransformsV2 -Wl,--no-whole-archive)
endif()

target_include_directories(test_transforms_v2_widget PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/TransformsV2_Widget>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

set_target_properties(test_transforms_v2_widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_transforms_v2_widget PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
else()
    add_test(NAME test_transforms_v2_widget_all
        COMMAND test_transforms_v2_widget
    )
endif()
