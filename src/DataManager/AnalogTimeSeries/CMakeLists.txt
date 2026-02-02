

set(analog_subdirectory_sources
    Analog_Time_Series.hpp
    Analog_Time_Series.cpp
    storage/AnalogDataStorage.hpp
    storage/AnalogDataStorage.cpp
    RaggedAnalogTimeSeries.hpp
    RaggedAnalogTimeSeries.cpp
    IO/Binary/Analog_Time_Series_Binary.hpp
    IO/Binary/Analog_Time_Series_Binary.cpp
    IO/CSV/Analog_Time_Series_CSV.hpp
    IO/CSV/Analog_Time_Series_CSV.cpp
    utils/statistics.hpp
    utils/statistics.cpp
)

add_library(AnalogTimeSeries STATIC ${analog_subdirectory_sources})

# Set up include directories
target_include_directories(AnalogTimeSeries PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For DataManager/utils and other utils
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>     # For IO subdirectories
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(AnalogTimeSeries PUBLIC
    WhiskerToolbox::ObserverData  # Observer
    TimeFrame                # TimeFrame  
    Entity                   # Entity
    nlohmann_json::nlohmann_json  # For JSON support
)

add_library(WhiskerToolbox::AnalogTimeSeries ALIAS AnalogTimeSeries)

set(analog_test_sources
    Analog_Time_Series.test.cpp
    IO/CSV/Analog_Time_Series_CSV.test.cpp
    utils/statistics.test.cpp)

add_tests_to_global(analog_test_sources
                    "${CMAKE_SOURCE_DIR}/src/DataManager/AnalogTimeSeries"
                    DATAMANAGER_TEST_SOURCES)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(AnalogTimeSeries PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(AnalogTimeSeries PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(AnalogTimeSeries PRIVATE ${MSVC_WARNINGS})
endif()