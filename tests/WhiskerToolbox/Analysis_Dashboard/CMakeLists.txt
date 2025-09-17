if (APPLE)
    message(STATUS "Testing Currenly not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currenly not supported on Windows")
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Xml Svg PrintSupport OpenGLWidgets OpenGL Test)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

# Test executable for AnalysisDashboard
add_executable(AnalysisDashboardTests
    test_analysis_dashboard.cpp
    PointDataVisualization.test.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Analysis_Dashboard/Analysis_Dashboard.test.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayOpenGLWidget.test.cpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.test.cpp
    ${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/Analysis_Dashboard/multiple_plot_groups.test.cpp
    ../fixtures/test_signal_probe_simple.cpp
    ../fixtures/signal_probe.cpp
    ../fixtures/simple_test_emitter.hpp
    ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/whiskertoolbox.qrc
)

# Enable automoc for files that contain Q_OBJECT
set_source_files_properties(../fixtures/test_signal_probe_simple.cpp PROPERTIES AUTOMOC TRUE)
set_source_files_properties(../fixtures/signal_probe.hpp PROPERTIES AUTOMOC TRUE)
set_source_files_properties(../fixtures/simple_test_emitter.hpp PROPERTIES AUTOMOC TRUE)

# Ensure MOC files are generated in the build directory
set_target_properties(AnalysisDashboardTests PROPERTIES
    AUTOMOC TRUE
)

target_link_libraries(AnalysisDashboardTests 
    PRIVATE 
    Analysis_Dashboard
    qt6advanceddocking
    DataManager
    TimeScrollBar
    ShaderManager
    Feature_Table_Widget
    Catch2::Catch2WithMain
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

#Turn off sanitize for this target

target_include_directories(AnalysisDashboardTests PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/Analysis_Dashboard>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/TimeScrollBar>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/ShaderManager>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/CoreGeometry>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/SpatialIndex>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tests/WhiskerToolbox/Analysis_Dashboard>"
)

catch_discover_tests(AnalysisDashboardTests PROPERTIES 
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)
