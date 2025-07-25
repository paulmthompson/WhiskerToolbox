
find_package(glm CONFIG REQUIRED)

set(DATAVIEWER_SOURCES
    XAxis.hpp
    PlottingManager/PlottingManager.hpp
    PlottingManager/PlottingManager.cpp

    AnalogTimeSeries/MVP_AnalogTimeSeries.hpp
    AnalogTimeSeries/MVP_AnalogTimeSeries.cpp
    AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp

    DigitalEvent/DigitalEventSeriesDisplayOptions.hpp
    DigitalEvent/MVP_DigitalEvent.cpp
    DigitalEvent/MVP_DigitalEvent.hpp

    DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp
    DigitalInterval/MVP_DigitalInterval.hpp
    DigitalInterval/MVP_DigitalInterval.cpp
)

set_property(GLOBAL PROPERTY DATAVIEWER_TEST_SOURCES "")

set(dataviewer_test_sources
    AnalogTimeSeries/MVP_AnalogTimeSeries.test.cpp
    DigitalEvent/MVP_DigitalEvent.test.cpp
    DigitalInterval/MVP_DigitalInterval.test.cpp
    )

add_tests_to_global(dataviewer_test_sources 
                    "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataViewer" 
                    DATAVIEWER_TEST_SOURCES)

add_library(DataViewer SHARED ${DATAVIEWER_SOURCES}

    )

target_link_libraries(DataViewer PRIVATE DataManager)

target_link_libraries(DataViewer PUBLIC glm::glm)

target_include_directories(DataViewer PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../DataManager>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataViewer PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataViewer PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataViewer PRIVATE ${MSVC_WARNINGS})
endif()

if(WIN32)
    set_target_properties(DataViewer PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()

set_target_properties(DataViewer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
