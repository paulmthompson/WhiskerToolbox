
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

add_executable(test_data_manager
        test_data_manager.cpp

        AnalogTimeSeries/test_analog_time_series_loader.cpp
        AnalogTimeSeries/test_analog_time_series.cpp

        DigitalTimeSeries/test_digital_event_series.cpp
        DigitalTimeSeries/test_digital_interval_series.cpp

        transforms/Masks/test_mask_area.cpp
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
