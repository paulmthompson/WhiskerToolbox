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

add_executable(test_media_widget
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Media_Widget/Media_Widget.test.cpp
)

target_link_libraries(test_media_widget PRIVATE 
    Catch2::Catch2WithMain 
    Media_Widget 
    DataManager 
    Qt6::Widgets
    Qt6::Core
)

target_include_directories(test_media_widget PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Media_Widget>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
)

# Enable Qt MOC for the test
set_target_properties(test_media_widget PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

catch_discover_tests(test_media_widget PROPERTIES 
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)
