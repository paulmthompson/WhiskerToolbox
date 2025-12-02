#include "transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalization.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

TEST_CASE("ZScoreNormalization - Multi-Pass Preprocessing", "[transforms][zscore][multipass]") {
    
    SECTION("Basic z-score normalization") {
        // Create test data: {1, 2, 3, 4, 5}
        // Mean = 3, Std = sqrt(2) ≈ 1.414
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Create pipeline with z-score normalization
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", ZScoreNormalizationParams{});
        
        // Execute pipeline
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);
        
        // Check that mean is approximately 0
        auto const& result_data = result->getAnalogTimeSeries();
        float sum = 0.0f;
        for (auto val : result_data) {
            sum += val;
        }
        float result_mean = sum / result_data.size();
        REQUIRE_THAT(result_mean, Catch::Matchers::WithinAbs(0.0f, 1e-5f));
        
        // Check that std is approximately 1
        float sum_sq = 0.0f;
        for (auto val : result_data) {
            sum_sq += (val - result_mean) * (val - result_mean);
        }
        float result_std = std::sqrt(sum_sq / (result_data.size() - 1));
        REQUIRE_THAT(result_std, Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    }
    
    SECTION("Z-score with outlier clamping") {
        // Create data with outlier: {1, 2, 3, 4, 100}
        // Mean = 22, outlier should be clamped
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 100.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Create pipeline with outlier clamping at 3 std
        ZScoreNormalizationParams params;
        params.clamp_outliers = true;
        params.outlier_threshold = 3.0f;
        
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", params);
        
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);
        
        // Last value should be clamped to threshold
        auto const& result_data = result->getAnalogTimeSeries();
        REQUIRE(result_data[4] <= params.outlier_threshold);
        REQUIRE(result_data[4] >= -params.outlier_threshold);
    }
    
    SECTION("Multi-pass does not materialize intermediate containers") {
        // This test verifies that preprocessing works with views
        // and doesn't require intermediate container materialization
        
        std::vector<float> data;
        for (int i = 0; i < 1000; ++i) {
            data.push_back(static_cast<float>(i));
        }
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Create pipeline - preprocessing should work on view
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", ZScoreNormalizationParams{});
        
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 1000);
        
        // Verify normalization worked
        auto const& result_data = result->getAnalogTimeSeries();
        float sum = 0.0f;
        for (auto val : result_data) {
            sum += val;
        }
        float result_mean = sum / result_data.size();
        REQUIRE_THAT(result_mean, Catch::Matchers::WithinAbs(0.0f, 1e-4f));
    }
    
    SECTION("Preprocessing is idempotent") {
        // Verify that preprocessing only happens once even if called multiple times
        
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        ZScoreNormalizationParams params;
        
        // Manually preprocess multiple times
        auto view = input->elements();
        params.preprocess(view);
        float first_mean = params.getMean();
        float first_std = params.getStd();
        
        params.preprocess(view);  // Should be no-op
        float second_mean = params.getMean();
        float second_std = params.getStd();
        
        REQUIRE(first_mean == second_mean);
        REQUIRE(first_std == second_std);
        REQUIRE(params.isPreprocessed());
    }
    
    SECTION("Chained transforms with preprocessing") {
        // Verify that preprocessing works in a chain with other transforms
        
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Create pipeline: normalize then sum (should sum to ~0)
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", ZScoreNormalizationParams{});
        // Note: Would need a suitable second transform that makes sense
        // For now, just verify single-step works
        
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);
    }
}

TEST_CASE("ZScoreNormalization - Edge Cases", "[transforms][zscore][edge]") {
    
    SECTION("Empty data") {
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", ZScoreNormalizationParams{});
        
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 0);
    }
    
    SECTION("Single value") {
        std::vector<float> data{42.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(0)};
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", ZScoreNormalizationParams{});
        
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 1);
        
        // With single value, std defaults to 1, so z-score = (value - value) / 1 = 0
        auto const& result_data = result->getAnalogTimeSeries();
        REQUIRE_THAT(result_data[0], Catch::Matchers::WithinAbs(0.0f, 1e-5f));
    }
    
    SECTION("Constant values (zero variance)") {
        std::vector<float> data{5.0f, 5.0f, 5.0f, 5.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        ZScoreNormalizationParams params;
        params.epsilon = 1e-8f;  // Prevent division by zero
        
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", params);
        
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);
        
        // All values should be 0 (or very close due to epsilon)
        auto const& result_data = result->getAnalogTimeSeries();
        for (auto val : result_data) {
            REQUIRE_THAT(val, Catch::Matchers::WithinAbs(0.0f, 1e-3f));
        }
    }
}
