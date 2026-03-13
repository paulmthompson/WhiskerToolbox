

add_library(RaggedTimeSeries INTERFACE)

target_include_directories(RaggedTimeSeries INTERFACE 
"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
"$<INSTALL_INTERFACE:include>")

target_link_libraries(RaggedTimeSeries INTERFACE 
    NEURALYZER_GEOMETRY      # CoreGeometry
    WhiskerToolbox::ObserverData  # Observer
    TimeFrame                # TimeFrame  
    Entity                   # Entity
    DataTypeTraits           # DataTypeTraits
)