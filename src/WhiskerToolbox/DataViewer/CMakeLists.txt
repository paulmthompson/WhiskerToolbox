
find_package(glm CONFIG REQUIRED)

set(DATAVIEWER_SOURCES

    AnalogTimeSeries/AnalogSeriesHelpers.hpp
    AnalogTimeSeries/AnalogSeriesHelpers.cpp
    AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp

    DigitalEvent/DigitalEventSeriesDisplayOptions.hpp

    DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp
)


add_library(DataViewer STATIC ${DATAVIEWER_SOURCES})

target_link_libraries(DataViewer PRIVATE WhiskerToolbox::AnalogTimeSeries)

target_link_libraries(DataViewer PUBLIC glm::glm)

target_include_directories(DataViewer PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../DataManager>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

set_target_compiler_warnings(DataViewer)

set_target_properties(DataViewer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
enable_whiskertoolbox_pch(DataViewer)