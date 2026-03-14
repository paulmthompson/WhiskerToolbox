find_package(reflectcpp CONFIG REQUIRED)

add_library(DataSynthesizer STATIC
    GeneratorTypes.hpp
    GeneratorRegistry.hpp
    GeneratorRegistry.cpp
    Registration.hpp
    Generators/Analog/SineWaveGenerator.cpp
    Generators/Analog/SquareWaveGenerator.cpp
    Generators/Analog/TriangleWaveGenerator.cpp
    Generators/Analog/SawtoothWaveGenerator.cpp
    Generators/Analog/GaussianNoiseGenerator.cpp
    Generators/Analog/UniformNoiseGenerator.cpp
)

add_library(WhiskerToolbox::DataSynthesizer ALIAS DataSynthesizer)

set_target_properties(DataSynthesizer
    PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

target_include_directories(DataSynthesizer PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(DataSynthesizer PUBLIC DataManager) # DataTypeVariant
target_link_libraries(DataSynthesizer PUBLIC WhiskerToolbox::ParameterSchema)
target_link_libraries(DataSynthesizer PUBLIC reflectcpp::reflectcpp)

target_link_libraries(DataSynthesizer PRIVATE WhiskerToolbox::AnalogTimeSeries)
#target_link_libraries(DataSynthesizer PRIVATE WhiskerToolbox::DigitalTimeSeries)
#target_link_libraries(DataSynthesizer PRIVATE WhiskerToolbox::PointData)
#target_link_libraries(DataSynthesizer PRIVATE WhiskerToolbox::LineData)
#target_link_libraries(DataSynthesizer PRIVATE WhiskerToolbox::MaskData)
#target_link_libraries(DataSynthesizer PRIVATE WhiskerToolbox::MediaData)

set_target_compiler_warnings(DataSynthesizer)
enable_whiskertoolbox_shared_pch(DataSynthesizer)
