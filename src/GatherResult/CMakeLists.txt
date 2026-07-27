
add_library(GatherResult INTERFACE)

target_include_directories(GatherResult
INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(GatherResult INTERFACE WhiskerToolbox::AnalogTimeSeries)
target_link_libraries(GatherResult INTERFACE WhiskerToolbox::DigitalTimeSeries)
target_link_libraries(GatherResult INTERFACE WhiskerToolbox::TimeFrame)
target_link_libraries(GatherResult INTERFACE TransformTypes)