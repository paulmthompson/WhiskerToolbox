if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_data_viewer_widget
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.test.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.test.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/whiskertoolbox.qrc
)

target_link_libraries(test_data_viewer_widget PRIVATE 
    Catch2::Catch2WithMain 
    DataViewer_Widget 
    DataViewer
    DataManager 
    PlottingOpenGL  # Contains ShaderManager
    Qt6::Widgets
    Qt6::Core
)

target_include_directories(test_data_viewer_widget PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataViewer_Widget>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/PlottingOpenGL>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
)

# Enable Qt MOC for the test
set_target_properties(test_data_viewer_widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_data_viewer_widget PROPERTIES 
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )

else()

    add_test(NAME test_data_viewer_widget_all
        COMMAND test_data_viewer_widget
        )
    set_property(TEST test_data_viewer_widget_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()