if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_plot_alignment_gather
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/Plots/Common/PlotAlignmentGather.test.cpp
)

target_link_libraries(test_plot_alignment_gather PRIVATE 
    Catch2::Catch2WithMain 
    DataManager
    PlotAlignmentWidget
    Qt6::Core
)

target_include_directories(test_plot_alignment_gather PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Plots/Common>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_plot_alignment_gather PROPERTIES 
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
else()
    add_test(NAME test_plot_alignment_gather COMMAND test_plot_alignment_gather)
endif()
