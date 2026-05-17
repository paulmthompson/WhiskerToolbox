add_executable(DataManagerPipelineRunner
        main.cpp
)

target_link_libraries(DataManagerPipelineRunner PRIVATE DataManager)
target_link_libraries(DataManagerPipelineRunner PRIVATE Commands)

# MediaData and DataSynthesizer both use file-scope registration objects.
# Force whole-archive linking so headless JSON workflows see the same registrations
# as the GUI and command tests.
if(APPLE)
    target_link_libraries(DataManagerPipelineRunner PRIVATE
            -Wl,-force_load,$<TARGET_FILE:MediaData>
            -Wl,-force_load,$<TARGET_FILE:DataSynthesizer>
    )
elseif(MSVC)
    target_link_options(DataManagerPipelineRunner PRIVATE
            /WHOLEARCHIVE:MediaData
            /WHOLEARCHIVE:DataSynthesizer
    )
    target_link_libraries(DataManagerPipelineRunner PRIVATE MediaData DataSynthesizer)
else()
    target_link_libraries(DataManagerPipelineRunner PRIVATE
            -Wl,--whole-archive MediaData DataSynthesizer -Wl,--no-whole-archive
    )
endif()

set_target_compiler_warnings(DataManagerPipelineRunner)

if(BUILD_TESTING)
    set(DATAMANAGER_PIPELINE_RUNNER_TEST_DIR
            "${CMAKE_CURRENT_BINARY_DIR}/test_configs")
    file(MAKE_DIRECTORY "${DATAMANAGER_PIPELINE_RUNNER_TEST_DIR}")
    file(WRITE
            "${DATAMANAGER_PIPELINE_RUNNER_TEST_DIR}/empty_pipeline.json"
            "{\n  \"commands\": []\n}\n")
    file(WRITE
            "${DATAMANAGER_PIPELINE_RUNNER_TEST_DIR}/missing_save_key.json"
            "{\n  \"saves\": [\n    {\n      \"data_key\": \"missing_points\",\n      \"format\": \"csv\",\n      \"path\": \"missing.csv\"\n    }\n  ]\n}\n")

    add_test(
            NAME DataManagerPipelineRunner_success_empty_pipeline
            COMMAND DataManagerPipelineRunner
                    "${DATAMANAGER_PIPELINE_RUNNER_TEST_DIR}/empty_pipeline.json")

    add_test(
            NAME DataManagerPipelineRunner_failure_missing_save_key
            COMMAND DataManagerPipelineRunner
                    "${DATAMANAGER_PIPELINE_RUNNER_TEST_DIR}/missing_save_key.json")
    set_tests_properties(
            DataManagerPipelineRunner_failure_missing_save_key
            PROPERTIES WILL_FAIL TRUE)
endif()
