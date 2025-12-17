
find_package(glm CONFIG REQUIRED)

set(DATAVIEWER_SOURCES

    AnalogTimeSeries/AnalogSeriesHelpers.hpp
    AnalogTimeSeries/AnalogSeriesHelpers.cpp
    AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp

    DigitalEvent/DigitalEventSeriesDisplayOptions.hpp

    DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp
)

set_property(GLOBAL PROPERTY DATAVIEWER_TEST_SOURCES "")

set(dataviewer_test_sources
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
