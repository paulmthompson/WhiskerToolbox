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

add_executable(test_io_widgets
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/IO_Widgets/Lines/Line_Loader_Widget.test.cpp
)

set_source_files_properties(${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/IO_Widgets/Lines/Line_Loader_Widget.test.cpp PROPERTIES AUTOMOC TRUE)

target_link_libraries(test_io_widgets PRIVATE 
    Catch2::Catch2WithMain 
    IO_Widget 
    DataManager 
    DataManagerIO
    DataManagerIO_CapnProto
    Qt6::Widgets
    Qt6::Core
    TensorData
)

target_include_directories(test_io_widgets PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/IO_Widgets>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
)

# Enable Qt MOC for the test
set_target_properties(test_io_widgets PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_io_widgets PROPERTIES 
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_io_widgets_all
        COMMAND test_io_widgets
        )
    set_property(TEST test_io_widgets_all PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()