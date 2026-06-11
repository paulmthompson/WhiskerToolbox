if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

if (NOT TARGET DataManagerPipelineRunner)
    message(FATAL_ERROR "DataManagerPipelineRunner target must be built before its tests")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

set(DATAMANAGER_PIPELINE_RUNNER_TEST_CONFIG_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/configs")

add_test(
        NAME DataManagerPipelineRunner_success_empty_pipeline
        COMMAND DataManagerPipelineRunner
                "${DATAMANAGER_PIPELINE_RUNNER_TEST_CONFIG_DIR}/empty_pipeline.json")

add_test(
        NAME DataManagerPipelineRunner_failure_missing_save_key
        COMMAND DataManagerPipelineRunner
                "${DATAMANAGER_PIPELINE_RUNNER_TEST_CONFIG_DIR}/missing_save_key.json")
set_tests_properties(
        DataManagerPipelineRunner_failure_missing_save_key
        PROPERTIES WILL_FAIL TRUE)
