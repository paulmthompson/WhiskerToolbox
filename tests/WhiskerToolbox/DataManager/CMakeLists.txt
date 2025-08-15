
if (APPLE)
    message(STATUS "Testing Currenly not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currenly not supported on Windows")
    return()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

get_property(DATAMANAGER_TEST_SOURCES_TEMP GLOBAL PROPERTY DATAMANAGER_TEST_SOURCES)
message(STATUS ${DATAMANAGER_TEST_SOURCES_TEMP})

add_executable(test_data_manager
        test_data_manager.cpp
        test_time_frame.cpp
        test_filename_timeframe.cpp
        test_tableview.cpp
        test_data_manager_fixtures.cpp

        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/Observer/Observer_Data.test.cpp

        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/DigitalTimeSeries/Digital_Event_Series.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/DigitalTimeSeries/Digital_Interval_Series.test.cpp

        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/utils/DataAggregation/DataAggregation.test.cpp

        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/utils/TableView/computers/EventInIntervalComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/utils/TableView/computers/IntervalOverlapComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/utils/TableView/computers/IntervalPropertyComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/utils/TableView/computers/LineSamplingMultiComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/utils/TableView/computers/AnalogTimestampOffsetsMultiComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/utils/TableView/computers/TimestampInIntervalComputer.test.cpp
        
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/utils/TableView/adapters/LineDataAdapter.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/utils/filter/NewFilterInterface.test.cpp

        ${DATAMANAGER_TEST_SOURCES_TEMP}

        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/AnalogTimeSeries/analog_event_threshold.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/AnalogTimeSeries/analog_filter.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/AnalogTimeSeries/analog_hilbert_phase.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/AnalogTimeSeries/analog_interval_threshold.test.cpp


        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/DigitalIntervalSeries/digital_interval_group.test.cpp

        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/Lines/line_angle.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/Lines/line_min_point_dist.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/Lines/line_alignment.test.cpp

        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/Masks/mask_area.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/Masks/mask_centroid.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/Masks/mask_connected_component.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/Masks/mask_hole_filling.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/Masks/mask_median_filter.test.cpp
        ${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager/transforms/Masks/mask_principal_axis.test.cpp
)

target_link_libraries(test_data_manager PRIVATE Catch2::Catch2WithMain DataManager)

target_include_directories(test_data_manager PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataManager>"
)

if (APPLE)
    set_target_properties(test_data_manager PROPERTIES
            BUILD_RPATH "${CMAKE_BINARY_DIR};${CMAKE_BINARY_DIR}/_deps/catch2-build/src"
            INSTALL_RPATH "@executable_path/../Frameworks"
    )
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

endif()

if (WIN32)
    catch_discover_tests(test_data_manager
            DL_PATHS
            "$ENV{PATH}"
            "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_data_manager)
endif()


file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data/Media)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data/Points)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data/Analog)

function(copy_files TARGET_NAME SOURCE_DIR DEST_DIR FILES)
    foreach(FILE ${FILES})
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                ${SOURCE_DIR}/${FILE}
                ${DEST_DIR}/${FILE})
    endforeach()
endfunction()

set(DATA_FILES_TO_COPY
        "test_config.json"
        "Media/test_each_frame_number.mp4"
        "Points/test_point.csv"
        "Analog/single_column.csv"
        "DigitalEvents/events.csv"
)

copy_files(test_data_manager
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        ${CMAKE_CURRENT_BINARY_DIR}/data
        "${DATA_FILES_TO_COPY}"
)
