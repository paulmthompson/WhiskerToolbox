

set(analog_subdirectory_sources
    Analog_Time_Series.hpp
    Analog_Time_Series.cpp
    storage/AnalogDataStorage.hpp
    storage/AnalogDataStorage.cpp
    storage/MemoryMappedAnalogDataStorage.hpp
    storage/MemoryMappedAnalogDataStorage.cpp

    RaggedAnalogTimeSeries.hpp
    RaggedAnalogTimeSeries.cpp
    
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
    DataTypeTraits           # DataTypeTraits
)

add_library(WhiskerToolbox::AnalogTimeSeries ALIAS AnalogTimeSeries)

set_target_compiler_warnings(AnalogTimeSeries)