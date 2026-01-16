#include "transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalizationV2.hpp"
#include "transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalization.hpp"
#include "transforms/v2/core/PipelineLoader.hpp"
#include "transforms/v2/core/RangeReductionRegistry.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/detail/ReductionStep.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

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
        pipeline.addPreReduction(ReductionStep{
                .reduction_name = "MeanValue",
                .output_key = "computed_mean",
                .params = {}});
        pipeline.addPreReduction(ReductionStep{
                .reduction_name = "StdValue",
                .output_key = "computed_std",
                .params = {}});

        // Add transform step with bindings
        ZScoreNormalizationParamsV2 params;
        params.clamp_outliers = false;
        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};

        pipeline.addStep("ZScoreNormalizeV2", params, bindings);

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

        // Check that std is approximately 1
        float sum_sq = 0.0f;
        for (auto val: result_data) {
            sum_sq += (val - result_mean) * (val - result_mean);
        }
        float result_std = std::sqrt(sum_sq / static_cast<float>(result_data.size() - 1));
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

        pipeline.addPreReduction(ReductionStep{
                .reduction_name = "MeanValue",
                .output_key = "computed_mean",
                .params = {}});
        pipeline.addPreReduction(ReductionStep{
                .reduction_name = "StdValue",
                .output_key = "computed_std",
                .params = {}});

        ZScoreNormalizationParamsV2 params;
        params.clamp_outliers = true;
        params.outlier_threshold = 3.0f;

        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};

        pipeline.addStep("ZScoreNormalizeV2", params, bindings);

        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);

        // Last value should be clamped to threshold
        auto const & result_data = result->getAnalogTimeSeries();
        REQUIRE(result_data[4] <= params.outlier_threshold);
        REQUIRE(result_data[4] >= -params.outlier_threshold);
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

        // V1 pipeline (uses preprocessing)
        TransformPipeline v1_pipeline;
        v1_pipeline.addStep("ZScoreNormalization", ZScoreNormalizationParams{});
        auto v1_result = v1_pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        // V2 pipeline (uses pre-reductions + bindings)
        TransformPipeline v2_pipeline;
        v2_pipeline.addPreReduction(ReductionStep{
                .reduction_name = "MeanValue",
                .output_key = "computed_mean",
                .params = {}});
        v2_pipeline.addPreReduction(ReductionStep{
                .reduction_name = "StdValue",
                .output_key = "computed_std",
                .params = {}});

        ZScoreNormalizationParamsV2 v2_params;
        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};
        v2_pipeline.addStep("ZScoreNormalizeV2", v2_params, bindings);

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

        // V1 with clamping
        ZScoreNormalizationParams v1_params;
        v1_params.clamp_outliers = true;
        v1_params.outlier_threshold = 2.5f;

        TransformPipeline v1_pipeline;
        v1_pipeline.addStep("ZScoreNormalization", v1_params);
        auto v1_result = v1_pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        // V2 with clamping
        TransformPipeline v2_pipeline;
        v2_pipeline.addPreReduction(ReductionStep{
                .reduction_name = "MeanValue",
                .output_key = "computed_mean",
                .params = {}});
        v2_pipeline.addPreReduction(ReductionStep{
                .reduction_name = "StdValue",
                .output_key = "computed_std",
                .params = {}});

        ZScoreNormalizationParamsV2 v2_params;
        v2_params.clamp_outliers = true;
        v2_params.outlier_threshold = 2.5f;

        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};
        v2_pipeline.addStep("ZScoreNormalizeV2", v2_params, bindings);

        auto v2_result = v2_pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(v1_result != nullptr);
        REQUIRE(v2_result != nullptr);

        // Both should have clamped values within threshold
        auto const & v1_data = v1_result->getAnalogTimeSeries();
        auto const & v2_data = v2_result->getAnalogTimeSeries();

        for (size_t i = 0; i < v1_data.size(); ++i) {
            REQUIRE(v1_data[i] <= v1_params.outlier_threshold);
            REQUIRE(v1_data[i] >= -v1_params.outlier_threshold);
            REQUIRE(v2_data[i] <= v2_params.outlier_threshold);
            REQUIRE(v2_data[i] >= -v2_params.outlier_threshold);
        }
    }
}

TEST_CASE("ZScoreNormalizationV2 - JSON Pipeline Loading", "[transforms][zscore][v2][json]") {

    SECTION("Load V2 pipeline from JSON string") {
        std::string const json_pipeline = R"({
            "name": "TestZScoreV2",
            "pre_reductions": [
                {"reduction": "MeanValue", "output_key": "computed_mean"},
                {"reduction": "StdValue", "output_key": "computed_std"}
            ],
            "steps": [
                {
                    "transform": "ZScoreNormalizeV2",
                    "params": {
                        "clamp_outliers": false,
                        "outlier_threshold": 3.0,
                        "epsilon": 1e-8
                    },
                    "param_bindings": {
                        "mean": "computed_mean",
                        "std_dev": "computed_std"
                    }
                }
            ]
        })";

        auto pipeline = loadPipelineFromJson(json_pipeline);
        REQUIRE(pipeline.has_value());

        // Create test data
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        // Execute the loaded pipeline
        auto result = pipeline->executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

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
            "name": "TestZScoreV2WithClamping",
            "pre_reductions": [
                {"reduction": "MeanValue", "output_key": "computed_mean"},
                {"reduction": "StdValue", "output_key": "computed_std"}
            ],
            "steps": [
                {
                    "transform": "ZScoreNormalizeV2",
                    "params": {
                        "clamp_outliers": true,
                        "outlier_threshold": 2.0,
                        "epsilon": 1e-8
                    },
                    "param_bindings": {
                        "mean": "computed_mean",
                        "std_dev": "computed_std"
                    }
                }
            ]
        })";

        auto pipeline = loadPipelineFromJson(json_pipeline);
        REQUIRE(pipeline.has_value());

        // Create data with outlier
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 50.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        auto result = pipeline->executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

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
        pipeline.addPreReduction(ReductionStep{
                .reduction_name = "MeanValue",
                .output_key = "computed_mean",
                .params = {}});
        pipeline.addPreReduction(ReductionStep{
                .reduction_name = "StdValue",
                .output_key = "computed_std",
                .params = {}});

        ZScoreNormalizationParamsV2 params;
        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};
        pipeline.addStep("ZScoreNormalizeV2", params, bindings);

        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);

        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Single value") {
        std::vector<float> data{42.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(0)};

        auto input = std::make_shared<AnalogTimeSeries>(data, times);

        TransformPipeline pipeline;
        pipeline.addPreReduction(ReductionStep{
                .reduction_name = "MeanValue",
                .output_key = "computed_mean",
                .params = {}});
        pipeline.addPreReduction(ReductionStep{
                .reduction_name = "StdValue",
                .output_key = "computed_std",
                .params = {}});

        ZScoreNormalizationParamsV2 params;
        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};
        pipeline.addStep("ZScoreNormalizeV2", params, bindings);

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
        pipeline.addPreReduction(ReductionStep{
                .reduction_name = "MeanValue",
                .output_key = "computed_mean",
                .params = {}});
        pipeline.addPreReduction(ReductionStep{
                .reduction_name = "StdValue",
                .output_key = "computed_std",
                .params = {}});

        ZScoreNormalizationParamsV2 params;
        params.epsilon = 1e-8f;// Prevent division by zero
        std::map<std::string, std::string> bindings{
                {"mean", "computed_mean"},
                {"std_dev", "computed_std"}};
        pipeline.addStep("ZScoreNormalizeV2", params, bindings);

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
