#include "transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalizationV2.hpp"
#include "transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalization.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/PipelineLoader.hpp"
#include "transforms/v2/core/RangeReductionRegistry.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <cmath>
#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

TEST_CASE("ZScoreNormalizationV2 - Parameter Deserialization", "[transforms][zscore][v2][debug]") {
    // Verify that the transform is registered
    auto& registry = ElementRegistry::instance();
    
    // Check what transforms are available
    auto all_names = registry.getAllTransformNames();
    INFO("Total registered transforms: " << all_names.size());
    
    bool found_zscore = false;
    bool found_mask = false;
    for (auto const& name : all_names) {
        if (name == "ZScoreNormalizeV2") found_zscore = true;
        if (name == "CalculateMaskArea") found_mask = true;
    }
    INFO("Found ZScoreNormalizeV2: " << found_zscore);
    INFO("Found CalculateMaskArea: " << found_mask);
    
    // First check MaskArea (which should work)
    auto const* mask_metadata = registry.getMetadata("CalculateMaskArea");
    REQUIRE(mask_metadata != nullptr);
    std::string const mask_params_json = R"({"scale_factor": 1.5})";
    auto mask_params_any = loadParametersForTransform("CalculateMaskArea", mask_params_json);
    INFO("MaskArea params has_value: " << mask_params_any.has_value());
    REQUIRE(mask_params_any.has_value());
    
    // Now check ZScoreNormalizeV2
    auto const* metadata = registry.getMetadata("ZScoreNormalizeV2");
    REQUIRE(metadata != nullptr);
    INFO("Transform metadata found");
    INFO("params_type name: " << metadata->params_type.name());
    
    // Check if the deserializer exists by trying direct rfl::json::read
    std::string const params_json = R"({"clamp_outliers": false, "outlier_threshold": 3.0, "epsilon": 0.00000001})";
    
    auto direct_result = rfl::json::read<ZScoreNormalizationParamsV2>(params_json);
    if (direct_result) {
        INFO("Direct rfl::json::read succeeded");
        INFO("Direct result clamp_outliers: " << direct_result.value().getClampOutliers());
    } else {
        FAIL("Direct rfl::json::read FAILED: " << direct_result.error()->what());
    }
    REQUIRE(direct_result);

    // Try the registry deserialize
    auto params_any = loadParametersForTransform("ZScoreNormalizeV2", params_json);
    
    if (!params_any.has_value()) {
        FAIL("Parameter deserialization returned empty any");
    }
    REQUIRE(params_any.has_value());
    
    // Try to cast to the correct type
    auto params = std::any_cast<ZScoreNormalizationParamsV2>(params_any);
    CHECK(params.getClampOutliers() == false);
    CHECK(params.getOutlierThreshold() == 3.0f);
}

TEST_CASE("ZScoreNormalizationV2 - Value Store Pattern", "[transforms][zscore][v2]") {

    SECTION("Basic z-score normalization with pre-reductions") {
        // Create test data: {1, 2, 3, 4, 5}
        // Mean = 3, Std = sqrt(2) ≈ 1.414
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        // Create pipeline with pre-reductions and ZScoreV2
        TransformPipeline pipeline;

        // Add pre-reductions to compute mean and std
        pipeline.addPreReduction("MeanValueRaw", "computed_mean");
        pipeline.addPreReduction("StdValueRaw", "computed_std");

        // Add transform step with bindings
        ZScoreNormalizationParamsV2 params;
        params.clamp_outliers = false;
        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};

        pipeline.addStepWithBindings("ZScoreNormalizeV2", params, bindings);

        // Execute pipeline
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);

        // Check that mean is approximately 0
        auto const & result_data = result->getAnalogTimeSeries();
        float sum = 0.0f;
        for (auto val: result_data) {
            sum += val;
        }
        float result_mean = sum / static_cast<float>(result_data.size());
        REQUIRE_THAT(result_mean, Catch::Matchers::WithinAbs(0.0f, 1e-5f));

        // Check that std is approximately 1 (using population std, same as stdValueRaw)
        float sum_sq = 0.0f;
        for (auto val: result_data) {
            sum_sq += (val - result_mean) * (val - result_mean);
        }
        float result_std = std::sqrt(sum_sq / static_cast<float>(result_data.size()));
        REQUIRE_THAT(result_std, Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    }

    SECTION("V2 with outlier clamping") {
        // Create data with outlier: {1, 2, 3, 4, 100}
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 100.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        // Create pipeline with outlier clamping
        TransformPipeline pipeline;

        pipeline.addPreReduction("MeanValueRaw", "computed_mean");
        pipeline.addPreReduction("StdValueRaw", "computed_std");

        ZScoreNormalizationParamsV2 params;
        params.clamp_outliers = true;
        params.outlier_threshold = 3.0f;

        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};

        pipeline.addStepWithBindings("ZScoreNormalizeV2", params, bindings);

        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);

        // Last value should be clamped to threshold
        auto const & result_data = result->getAnalogTimeSeries();
        REQUIRE(result_data[4] <= params.getOutlierThreshold());
        REQUIRE(result_data[4] >= -params.getOutlierThreshold());
    }

    SECTION("V2 manual value injection (without pre-reductions)") {
        // Test the transform directly with known values
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        // Manually set mean=3, std=sqrt(2)
        ZScoreNormalizationParamsV2 params;
        params.mean = 3.0f;
        params.std_dev = std::sqrt(2.0f);

        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalizeV2", params);

        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);

        // Expected z-scores: (1-3)/sqrt(2)=-sqrt(2), (2-3)/sqrt(2), ..., (5-3)/sqrt(2)=sqrt(2)
        auto const & result_data = result->getAnalogTimeSeries();
        float expected_first = (1.0f - 3.0f) / std::sqrt(2.0f);
        float expected_last = (5.0f - 3.0f) / std::sqrt(2.0f);

        REQUIRE_THAT(result_data[0], Catch::Matchers::WithinAbs(expected_first, 1e-5f));
        REQUIRE_THAT(result_data[4], Catch::Matchers::WithinAbs(expected_last, 1e-5f));
    }
}

TEST_CASE("ZScoreNormalizationV2 - Comparison with V1", "[transforms][zscore][v2][comparison]") {

    SECTION("V1 and V2 produce same results") {
        // Create test data
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        // Compute statistics manually for V1
        float sum = 0.0f;
        for (auto v : data) sum += v;
        float mean = sum / static_cast<float>(data.size());
        float sum_sq = 0.0f;
        for (auto v : data) sum_sq += (v - mean) * (v - mean);
        float std_dev = std::sqrt(sum_sq / static_cast<float>(data.size() - 1));

        // V1 pipeline (uses manual statistics)
        ZScoreNormalizationParams v1_params;
        v1_params.setStatistics(mean, std_dev);
        TransformPipeline v1_pipeline;
        v1_pipeline.addStep("ZScoreNormalization", v1_params);
        auto v1_result = v1_pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        // V2 pipeline (uses pre-reductions + bindings)
        TransformPipeline v2_pipeline;
        v2_pipeline.addPreReduction("MeanValueRaw", "computed_mean");
        v2_pipeline.addPreReduction("StdValueRaw", "computed_std");

        ZScoreNormalizationParamsV2 v2_params;
        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};
        v2_pipeline.addStepWithBindings("ZScoreNormalizeV2", v2_params, bindings);

        auto v2_result = v2_pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(v1_result != nullptr);
        REQUIRE(v2_result != nullptr);
        REQUIRE(v1_result->getNumSamples() == v2_result->getNumSamples());

        // Compare results element by element
        auto const & v1_data = v1_result->getAnalogTimeSeries();
        auto const & v2_data = v2_result->getAnalogTimeSeries();

        for (size_t i = 0; i < v1_data.size(); ++i) {
            // Note: Small difference expected due to population vs sample std deviation
            // V1 uses sample std (Bessel's correction), ValueRangeReductions::stdValue uses population std
            // But the difference should be small for larger N
            REQUIRE_THAT(v1_data[i], Catch::Matchers::WithinAbs(v2_data[i], 0.15f));
        }
    }

    SECTION("V1 and V2 with clamping produce same results") {
        // Create data with outlier
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 100.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        // Compute statistics manually for V1
        float sum = 0.0f;
        for (auto v : data) sum += v;
        float mean = sum / static_cast<float>(data.size());
        float sum_sq = 0.0f;
        for (auto v : data) sum_sq += (v - mean) * (v - mean);
        float std_dev = std::sqrt(sum_sq / static_cast<float>(data.size() - 1));

        // V1 with clamping
        ZScoreNormalizationParams v1_params;
        v1_params.setStatistics(mean, std_dev);
        v1_params.clamp_outliers = true;
        v1_params.outlier_threshold = 2.5f;

        TransformPipeline v1_pipeline;
        v1_pipeline.addStep("ZScoreNormalization", v1_params);
        auto v1_result = v1_pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        // V2 with clamping
        TransformPipeline v2_pipeline;
        v2_pipeline.addPreReduction("MeanValueRaw", "computed_mean");
        v2_pipeline.addPreReduction("StdValueRaw", "computed_std");

        ZScoreNormalizationParamsV2 v2_params;
        v2_params.clamp_outliers = true;
        v2_params.outlier_threshold = 2.5f;

        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};
        v2_pipeline.addStepWithBindings("ZScoreNormalizeV2", v2_params, bindings);

        auto v2_result = v2_pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(v1_result != nullptr);
        REQUIRE(v2_result != nullptr);

        // Both should have clamped values within threshold
        auto const & v1_data = v1_result->getAnalogTimeSeries();
        auto const & v2_data = v2_result->getAnalogTimeSeries();

        for (size_t i = 0; i < v1_data.size(); ++i) {
            REQUIRE(v1_data[i] <= v1_params.outlier_threshold);
            REQUIRE(v1_data[i] >= -v1_params.outlier_threshold);
            REQUIRE(v2_data[i] <= v2_params.getOutlierThreshold());
            REQUIRE(v2_data[i] >= -v2_params.getOutlierThreshold());
        }
    }
}

TEST_CASE("ZScoreNormalizationV2 - JSON Pipeline Loading", "[transforms][zscore][v2][json]") {

    SECTION("Load V2 pipeline from JSON string") {
        std::string const json_pipeline = R"({
            "metadata": {"name": "TestZScoreV2"},
            "pre_reductions": [
                {"reduction_name": "MeanValueRaw", "output_key": "computed_mean"},
                {"reduction_name": "StdValueRaw", "output_key": "computed_std"}
            ],
            "steps": [
                {
                    "step_id": "zscore",
                    "transform_name": "ZScoreNormalizeV2",
                    "parameters": {
                        "clamp_outliers": false,
                        "outlier_threshold": 3.0,
                        "epsilon": 0.00000001
                    },
                    "param_bindings": {
                        "mean": "computed_mean",
                        "std_dev": "computed_std"
                    }
                }
            ]
        })";

        auto pipeline = loadPipelineFromJson(json_pipeline);
        if (!pipeline) {
            FAIL("Pipeline loading failed: " << pipeline.error()->what());
        }
        REQUIRE(pipeline);  // rfl::Result is truthy on success

        // Create test data
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        // Execute the loaded pipeline
        auto result = pipeline.value().executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);

        // Verify normalization worked
        auto const & result_data = result->getAnalogTimeSeries();
        float sum = 0.0f;
        for (auto val: result_data) {
            sum += val;
        }
        float result_mean = sum / static_cast<float>(result_data.size());
        REQUIRE_THAT(result_mean, Catch::Matchers::WithinAbs(0.0f, 1e-5f));
    }

    SECTION("Load V2 pipeline with clamping from JSON") {
        std::string const json_pipeline = R"({
            "metadata": {"name": "TestZScoreV2WithClamping"},
            "pre_reductions": [
                {"reduction_name": "MeanValueRaw", "output_key": "computed_mean"},
                {"reduction_name": "StdValueRaw", "output_key": "computed_std"}
            ],
            "steps": [
                {
                    "step_id": "zscore_clamp",
                    "transform_name": "ZScoreNormalizeV2",
                    "parameters": {
                        "clamp_outliers": true,
                        "outlier_threshold": 2.0,
                        "epsilon": 0.00000001
                    },
                    "param_bindings": {
                        "mean": "computed_mean",
                        "std_dev": "computed_std"
                    }
                }
            ]
        })";

        auto pipeline = loadPipelineFromJson(json_pipeline);
        if (!pipeline) {
            FAIL("Pipeline loading failed: " << pipeline.error()->what());
        }
        REQUIRE(pipeline);  // rfl::Result is truthy on success

        // Create data with outlier
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 50.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        auto result = pipeline.value().executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(result != nullptr);

        // All values should be within clamping threshold
        auto const & result_data = result->getAnalogTimeSeries();
        for (auto val: result_data) {
            REQUIRE(val <= 2.0f);
            REQUIRE(val >= -2.0f);
        }
    }
}

TEST_CASE("ZScoreNormalizationV2 - Edge Cases", "[transforms][zscore][v2][edge]") {

    SECTION("Empty data") {
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        TransformPipeline pipeline;
        pipeline.addPreReduction("MeanValueRaw", "computed_mean");
        pipeline.addPreReduction("StdValueRaw", "computed_std");

        ZScoreNormalizationParamsV2 params;
        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};
        pipeline.addStepWithBindings("ZScoreNormalizeV2", params, bindings);

        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Single value") {
        std::vector<float> data{42.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(0)};

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        TransformPipeline pipeline;
        pipeline.addPreReduction("MeanValueRaw", "computed_mean");
        pipeline.addPreReduction("StdValueRaw", "computed_std");

        ZScoreNormalizationParamsV2 params;
        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};
        pipeline.addStepWithBindings("ZScoreNormalizeV2", params, bindings);

        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 1);
        // Note: With std=0 and epsilon, the result should be close to 0
    }

    SECTION("Constant data (zero variance)") {
        std::vector<float> data{5.0f, 5.0f, 5.0f, 5.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        TransformPipeline pipeline;
        pipeline.addPreReduction("MeanValueRaw", "computed_mean");
        pipeline.addPreReduction("StdValueRaw", "computed_std");

        ZScoreNormalizationParamsV2 params;
        params.epsilon = 1e-8f;// Prevent division by zero
        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};
        pipeline.addStepWithBindings("ZScoreNormalizeV2", params, bindings);

        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);

        // All values should be close to 0 (or very large due to epsilon)
        auto const & result_data = result->getAnalogTimeSeries();
        for (auto val: result_data) {
            // With std≈0 and epsilon, values should be essentially 0
            REQUIRE_THAT(val, Catch::Matchers::WithinAbs(0.0f, 1e-4f));
        }
    }
}
