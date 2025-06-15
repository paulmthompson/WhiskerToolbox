

set(analog_subdirectory_sources
    Analog_Time_Series.hpp
    Analog_Time_Series.cpp
    IO/Binary/Analog_Time_Series_Binary.hpp
    IO/Binary/Analog_Time_Series_Binary.cpp
    IO/CSV/Analog_Time_Series_CSV.hpp
    IO/CSV/Analog_Time_Series_CSV.cpp
    IO/JSON/Analog_Time_Series_JSON.hpp
    IO/JSON/Analog_Time_Series_JSON.cpp
    utils/statistics.hpp
    utils/statistics.cpp
)

set(analog_test_sources
    Analog_Time_Series.test.cpp
    IO/CSV/Analog_Time_Series_CSV.test.cpp
    utils/statistics.test.cpp)

prefix_list_items(analog_subdirectory_sources "AnalogTimeSeries")
set(DATAMANAGER_SOURCES ${DATAMANAGER_SOURCES} ${analog_subdirectory_sources} PARENT_SCOPE)

add_tests_to_global(analog_test_sources
                    "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/AnalogTimeSeries"
                    DATAMANAGER_TEST_SOURCES)
