

set(analog_subdirectory_sources
    Analog_Time_Series.hpp
    Analog_Time_Series.cpp
    IO/Binary/Analog_Time_Series_Binary.hpp
    IO/Binary/Analog_Time_Series_Binary.cpp
    IO/CSV/Analog_Time_Series_CSV.hpp
    IO/CSV/Analog_Time_Series_CSV.cpp
    IO/JSON/Analog_Time_Series_JSON.hpp
    IO/JSON/Analog_Time_Series_JSON.cpp
)

# Prepare a list of sources with paths relative to the PARENT CMakeLists.txt
set(sources_to_add_to_parent "")
foreach(source_file ${analog_subdirectory_sources})
    list(APPEND sources_to_add_to_parent "AnalogTimeSeries/${source_file}")
endforeach()

set(DATAMANAGER_SOURCES ${DATAMANAGER_SOURCES} ${sources_to_add_to_parent} PARENT_SCOPE)
