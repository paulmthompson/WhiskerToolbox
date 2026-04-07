if (APPLE)
    message(STATUS "Testing Currenly not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currenly not supported on Windows")
    return()
endif()

add_executable(test_TransformsV2

        test_view_pipeline.test.cpp
        test_line_min_point_dist_v2.test.cpp
        #test_zipping_logic.test.cpp
        test_pipeline_adaptors.test.cpp
        test_pipeline_loader.test.cpp
        test_pipeline_zipping.test.cpp
        test_mask_to_interval_pipeline.test.cpp
        test_multi_input_pipeline.test.cpp
        test_range_reduction_types.test.cpp
        test_range_reduction_registry.test.cpp
        test_range_reductions.test.cpp
        test_context_aware_params.test.cpp
        test_value_projection_types.test.cpp
        test_pipeline_value_store.test.cpp
        test_parameter_binding.test.cpp
        test_parameter_schema.test.cpp
        test_transform_lineage_integration.test.cpp
        test_tensor_column_builders.test.cpp
        test_nonanalog_column_builders.test.cpp
        test_phase1_3_4_reductions_transforms.test.cpp
        test_multistep_column_builders.test.cpp
        test_executor_timekey.test.cpp

    ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/AnalogEventThreshold/AnalogEventThreshold.test.cpp
    ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/AnalogHilbertPhase/AnalogHilbertPhase.test.cpp
    ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/AnalogIntervalPeak/AnalogIntervalPeak.test.cpp
    ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/AnalogIntervalThreshold/AnalogIntervalThreshold.test.cpp
    ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/DigitalIntervalBoolean/DigitalIntervalBoolean.test.cpp
    ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/IntervalReduction/IntervalReduction.test.cpp
        ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/LineAngle/LineAngle.test.cpp
        ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/LineBaseFlip/LineBaseFlip.test.cpp
        ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/LineClip/LineClip.test.cpp
        ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/LineCurvature/LineCurvature.test.cpp
        #${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/LineMinPointDist/LineMinPointDist.test.cpp
        ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/LinePointExtraction/LinePointExtraction.test.cpp
        ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/LineResample/LineResample.test.cpp
        ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/LineSubsegment/LineSubsegment.test.cpp
        ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/MaskArea/MaskArea.test.cpp
        ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/MaskCentroid/MaskCentroid.test.cpp
        #Range Reductions
        #SumReduction
        ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/ZScoreNormalization/ZScoreNormalizationV2.test.cpp
    ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/TensorPCA/TensorPCA.test.cpp
    ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/TensorTSNE/TensorTSNE.test.cpp
    ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/TensorRobustPCA/TensorRobustPCA.test.cpp
    ${CMAKE_SOURCE_DIR}/src/TransformsV2/algorithms/SincInterpolation/SincInterpolation.test.cpp
)

target_include_directories(test_TransformsV2 PRIVATE

        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/TransformsV2>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tests/DataManager>"
    )

target_link_libraries(test_TransformsV2 PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test_TransformsV2 PRIVATE DataManager)
target_link_libraries(test_TransformsV2 PRIVATE WhiskerToolbox::TensorData)

# Force whole-archive linking for TransformsV2 to ensure static transform registration
# (RegisteredTransforms.cpp has file-scope RAII objects that register transforms into
# ElementRegistry, but the linker would strip them since tests have no direct symbol references)
if(APPLE)
    target_link_libraries(test_TransformsV2 PRIVATE -Wl,-force_load,$<TARGET_FILE:TransformsV2>)
elseif(MSVC)
    target_link_options(test_TransformsV2 PRIVATE /WHOLEARCHIVE:TransformsV2)
    target_link_libraries(test_TransformsV2 PRIVATE TransformsV2)
else()
    # Linux/GCC/Clang
    target_link_libraries(test_TransformsV2 PRIVATE -Wl,--whole-archive TransformsV2 -Wl,--no-whole-archive)
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_TransformsV2
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_TransformsV2)
    endif()
else()
    add_test(NAME test_TransformsV2_all
        COMMAND test_TransformsV2
        )
endif()

set_target_compiler_warnings(test_TransformsV2)

enable_whiskertoolbox_shared_test_pch(test_TransformsV2)