
set(digital_subdirectory_sources
        Digital_Interval_Series.hpp
        Digital_Interval_Series.cpp
        Digital_Event_Series.hpp
        Digital_Event_Series.cpp
)

add_library(DigitalTimeSeries STATIC ${digital_subdirectory_sources})
set_target_properties(DigitalTimeSeries PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Set up include directories
target_include_directories(DigitalTimeSeries PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For DataManager/utils and other utils
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>     # For IO subdirectories
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(DigitalTimeSeries PUBLIC
    WhiskerToolbox::ObserverData  # Observer
    TimeFrame                # TimeFrame  
    Entity                   # Entity
    nlohmann_json::nlohmann_json  # For JSON support
)

add_library(WhiskerToolbox::DigitalTimeSeries ALIAS DigitalTimeSeries)

set(digital_test_sources

    )

add_tests_to_global(digital_test_sources
                    "${CMAKE_SOURCE_DIR}/src/DataManager/DigitalTimeSeries"
                    DATAMANAGER_TEST_SOURCES)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DigitalTimeSeries PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DigitalTimeSeries PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DigitalTimeSeries PRIVATE ${MSVC_WARNINGS})
endif()