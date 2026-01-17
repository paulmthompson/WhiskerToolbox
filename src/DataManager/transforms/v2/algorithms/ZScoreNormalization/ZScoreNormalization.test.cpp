#include "transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalization.hpp"
#include "transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalizationV2.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

// Helper function to compute mean of a vector
inline float computeMean(std::vector<float> const& data) {
    if (data.empty()) return 0.0f;
    return std::accumulate(data.begin(), data.end(), 0.0f) / data.size();
}

// Helper function to compute sample standard deviation
inline float computeStd(std::vector<float> const& data, float mean) {
    if (data.size() <= 1) return 1.0f;  // Default to 1 for single value
    float sum_sq = 0.0f;
    for (auto val : data) {
        sum_sq += (val - mean) * (val - mean);
    }
    return std::sqrt(sum_sq / (data.size() - 1));  // Sample std
}

TEST_CASE("ZScoreNormalization - Manual Statistics", "[transforms][zscore]") {
    
    SECTION("Basic z-score normalization with manual statistics") {
        // Create test data: {1, 2, 3, 4, 5}
        // Mean = 3, Std = sqrt(2.5) ≈ 1.581
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Compute statistics manually
        float mean = computeMean(data);
        float std_dev = computeStd(data, mean);
        
        // Create params with manual statistics
        ZScoreNormalizationParams params;
        params.setStatistics(mean, std_dev);
        
        // Create pipeline with z-score normalization
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", params);
        
        // Execute pipeline
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);
        
        // Check that mean is approximately 0
        auto const& result_data = result->getAnalogTimeSeries();
        float result_mean = computeMean(std::vector<float>(result_data.begin(), result_data.end()));
        REQUIRE_THAT(result_mean, Catch::Matchers::WithinAbs(0.0f, 1e-5f));
        
        // Check that std is approximately 1
        float result_std = computeStd(std::vector<float>(result_data.begin(), result_data.end()), result_mean);
        REQUIRE_THAT(result_std, Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    }
    
    SECTION("Z-score with outlier clamping") {
        // Create data with outlier: {1, 2, 3, 4, 100}
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 100.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Compute statistics manually
        float mean = computeMean(data);
        float std_dev = computeStd(data, mean);
        
        // Create pipeline with outlier clamping at 3 std
        ZScoreNormalizationParams params;
        params.setStatistics(mean, std_dev);
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
    
    SECTION("Large dataset with manual statistics") {
        std::vector<float> data;
        for (int i = 0; i < 1000; ++i) {
            data.push_back(static_cast<float>(i));
        }
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Compute statistics
        float mean = computeMean(data);
        float std_dev = computeStd(data, mean);
        
        ZScoreNormalizationParams params;
        params.setStatistics(mean, std_dev);
        
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", params);
        
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 1000);
        
        // Verify normalization worked
        auto const& result_data = result->getAnalogTimeSeries();
        float result_mean = computeMean(std::vector<float>(result_data.begin(), result_data.end()));
        REQUIRE_THAT(result_mean, Catch::Matchers::WithinAbs(0.0f, 1e-4f));
    }
    
    SECTION("Manual statistics setting") {
        // Verify that manually setting statistics works correctly
        
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        // Manually set statistics (mean=3, std~=1.414)
        ZScoreNormalizationParams params;
        params.setStatistics(3.0f, std::sqrt(2.0f));
        
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", params);
        
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 5);
        
        // Check that the transform used the manually set statistics
        auto const& result_data = result->getAnalogTimeSeries();
        // First value: (1-3)/sqrt(2) ≈ -1.414
        REQUIRE_THAT(result_data[0], Catch::Matchers::WithinAbs(-std::sqrt(2.0f), 1e-4f));
        // Third value: (3-3)/sqrt(2) = 0
        REQUIRE_THAT(result_data[2], Catch::Matchers::WithinAbs(0.0f, 1e-5f));
    }
    
    SECTION("Chained transforms with manual statistics") {
        // Verify that setting statistics works in a pipeline
        
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (size_t i = 0; i < data.size(); ++i) {
            times.emplace_back(i);
        }
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        ZScoreNormalizationParams params;
        params.setStatistics(3.0f, std::sqrt(2.0f));  // Pre-set statistics
        
        // Create pipeline with z-score normalization
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", params);
        
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
        
        // Even for empty data, we need to set statistics
        ZScoreNormalizationParams params;
        params.setStatistics(0.0f, 1.0f);
        
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", params);
        
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 0);
    }
    
    SECTION("Single value") {
        std::vector<float> data{42.0f};
        std::vector<TimeFrameIndex> times{TimeFrameIndex(0)};
        
        auto input = std::make_shared<AnalogTimeSeries>(data, times);
        
        // For single value, std would be 0 or undefined, so use 1
        ZScoreNormalizationParams params;
        params.setStatistics(42.0f, 1.0f);
        
        TransformPipeline pipeline;
        pipeline.addStep("ZScoreNormalization", params);
        
        auto result = pipeline.executeOptimized<AnalogTimeSeries, AnalogTimeSeries>(*input);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getNumSamples() == 1);
        
        // With single value, std=1, so z-score = (42 - 42) / 1 = 0
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
        // For constant values, std=0, so we use epsilon
        params.setStatistics(5.0f, params.epsilon);
        
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
