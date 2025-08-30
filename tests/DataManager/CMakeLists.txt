
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

        test_filename_timeframe.cpp
        test_tableview.cpp
        test_data_manager_fixtures.cpp

        ${CMAKE_SOURCE_DIR}/src/DataManager/Observer/Observer_Data.test.cpp

        ${CMAKE_SOURCE_DIR}/src/DataManager/DigitalTimeSeries/Digital_Event_Series.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/DigitalTimeSeries/Digital_Interval_Series.test.cpp

        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/DataAggregation/DataAggregation.test.cpp

        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/TableView/computers/AnalogSliceGathererComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/TableView/computers/EventInIntervalComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/TableView/computers/IntervalOverlapComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/TableView/computers/IntervalPropertyComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/TableView/computers/IntervalReductionComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/TableView/computers/LineSamplingMultiComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/TableView/computers/AnalogTimestampOffsetsMultiComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/TableView/computers/TimestampInIntervalComputer.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/TableView/computers/TimestampValueComputer.test.cpp

        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/TableView/adapters/LineDataAdapter.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/utils/filter/NewFilterInterface.test.cpp

        # Integration tests in tests/DataManager/TableView/
        TableView/multisample_data.test.cpp

        ${DATAMANAGER_TEST_SOURCES_TEMP}

        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/AnalogTimeSeries/AnalogFilter/analog_filter.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/AnalogTimeSeries/Analog_Interval_Threshold/analog_interval_threshold.test.cpp

        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/DigitalIntervalSeries/Digital_Interval_Group/digital_interval_group.test.cpp

        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Lines/Line_Alignment/line_alignment.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Lines/Line_Angle/line_angle.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Lines/Line_Clip/line_clip.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Lines/Line_Min_Point_Dist/line_min_point_dist.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Lines/Line_Resample/line_resample.test.cpp

        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Masks/Mask_Area/mask_area.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Masks/Mask_Centroid/mask_centroid.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Masks/Mask_Connected_Component/mask_connected_component.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Masks/Mask_Hole_Filling/mask_hole_filling.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Masks/Mask_Median_Filter/mask_median_filter.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Masks/Mask_Principal_Axis/mask_principal_axis.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Masks/Mask_Skeletonize/mask_skeletonize.test.cpp
        ${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Masks/Mask_To_Line/mask_to_line.test.cpp

        # IO tests
        IO/line_data_csv.test.cpp
        IO/analog_time_series_csv.test.cpp
        IO/digital_interval_series_csv.test.cpp
)

# Add VideoData tests only if FFmpeg is enabled
if(ENABLE_FFMPEG)
    target_sources(test_data_manager PRIVATE test_video_data_manager.cpp)
endif()

if(ENABLE_CAPNPROTO)
    target_sources(test_data_manager PRIVATE IO/line_data_binary.test.cpp)
    target_link_libraries(test_data_manager PRIVATE DataManagerIO_CapnProto)
    target_compile_definitions(test_data_manager PRIVATE ENABLE_CAPNPROTO)
endif()

if(ENABLE_HDF5)
    target_sources(test_data_manager PRIVATE IO/line_data_hdf5.test.cpp)
    target_link_libraries(test_data_manager PRIVATE DataManagerHDF5)
    target_compile_definitions(test_data_manager PRIVATE ENABLE_HDF5)
endif()

if(ENABLE_OPENCV)
    target_sources(test_data_manager PRIVATE IO/mask_data_image.test.cpp)
    target_link_libraries(test_data_manager PRIVATE DataManagerOpenCV)
    target_compile_definitions(test_data_manager PUBLIC ENABLE_OPENCV)
endif()

target_link_libraries(test_data_manager PRIVATE Catch2::Catch2WithMain DataManager)
target_link_libraries(test_data_manager PRIVATE DataManagerIO)

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
