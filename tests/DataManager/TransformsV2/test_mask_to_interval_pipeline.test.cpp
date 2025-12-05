/**
 * @file test_mask_to_interval_pipeline.test.cpp
 * @brief Integration test for multi-step pipeline: MaskData → Area → Sum → IntervalThreshold
 *
 * This test demonstrates chaining multiple transforms via DataManagerPipelineExecutor:
 * 1. CalculateMaskArea: MaskData → RaggedAnalogTimeSeries (area per mask)
 * 2. SumReduction: RaggedAnalogTimeSeries → AnalogTimeSeries (sum areas at each time)
 * 3. AnalogIntervalThreshold: AnalogTimeSeries → DigitalIntervalSeries (threshold crossings)
 *
 * The entire pipeline is specified in a single JSON configuration and executed
 * through the DataManagerPipelineExecutor.
 */

#include "transforms/v2/algorithms/AnalogIntervalThreshold/AnalogIntervalThreshold.hpp"
#include "transforms/v2/algorithms/MaskArea/MaskArea.hpp"
#include "transforms/v2/algorithms/SumReduction/SumReduction.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "CoreGeometry/masks.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

// ============================================================================
// Test Fixture: Creates MaskData with varying areas across time
// ============================================================================

class MaskToIntervalPipelineFixture {
protected:
    MaskToIntervalPipelineFixture() {
        m_data_manager = std::make_unique<DataManager>();
        
        // Create time frame: 0, 100, 200, 300, 400, 500, 600, 700, 800, 900
        std::vector<int> times;
        for (int t = 0; t <= 900; t += 100) {
            times.push_back(t);
        }
        m_time_frame = std::make_shared<TimeFrame>(times);
        m_data_manager->setTime(TimeKey("default"), m_time_frame);
        
        createTestMaskData();
    }

    DataManager* getDataManager() { return m_data_manager.get(); }

private:
    /**
     * @brief Create mask data with varying total areas across time
     * 
     * Creates masks such that the total area at each time point is:
     * - Time 0:   area = 5  (below threshold of 10)
     * - Time 100: area = 8  (below threshold)
     * - Time 200: area = 15 (above threshold) -- START of interval 1
     * - Time 300: area = 20 (above threshold)
     * - Time 400: area = 12 (above threshold) -- END of interval 1
     * - Time 500: area = 6  (below threshold)
     * - Time 600: area = 3  (below threshold)
     * - Time 700: area = 18 (above threshold) -- START of interval 2
     * - Time 800: area = 25 (above threshold)
     * - Time 900: area = 4  (below threshold) -- END of interval 2
     * 
     * Expected intervals with threshold=10, direction=positive:
     * - [200, 400] (indices 2-4)
     * - [700, 800] (indices 7-8)
     */
    void createTestMaskData() {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(m_time_frame);

        // Helper to create a mask with N pixels
        auto create_mask = [](int num_pixels) {
            std::vector<Point2D<uint32_t>> points;
            for (int i = 0; i < num_pixels; ++i) {
                points.push_back(Point2D<uint32_t>{
                    static_cast<uint32_t>(i % 10),
                    static_cast<uint32_t>(i / 10)
                });
            }
            return Mask2D(points);
        };

        // Time 0: single mask with 5 pixels (total area = 5)
        mask_data->addAtTime(TimeFrameIndex(0), create_mask(5), NotifyObservers::No);

        // Time 100: two masks: 3 + 5 = 8 pixels (total area = 8)
        mask_data->addAtTime(TimeFrameIndex(1), create_mask(3), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(1), create_mask(5), NotifyObservers::No);

        // Time 200: two masks: 10 + 5 = 15 pixels (total area = 15, ABOVE threshold)
        mask_data->addAtTime(TimeFrameIndex(2), create_mask(10), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(2), create_mask(5), NotifyObservers::No);

        // Time 300: single mask with 20 pixels (total area = 20, ABOVE threshold)
        mask_data->addAtTime(TimeFrameIndex(3), create_mask(20), NotifyObservers::No);

        // Time 400: three masks: 4 + 5 + 3 = 12 pixels (total area = 12, ABOVE threshold)
        mask_data->addAtTime(TimeFrameIndex(4), create_mask(4), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(4), create_mask(5), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(4), create_mask(3), NotifyObservers::No);

        // Time 500: single mask with 6 pixels (total area = 6, below threshold)
        mask_data->addAtTime(TimeFrameIndex(5), create_mask(6), NotifyObservers::No);

        // Time 600: single mask with 3 pixels (total area = 3, below threshold)
        mask_data->addAtTime(TimeFrameIndex(6), create_mask(3), NotifyObservers::No);

        // Time 700: two masks: 10 + 8 = 18 pixels (total area = 18, ABOVE threshold)
        mask_data->addAtTime(TimeFrameIndex(7), create_mask(10), NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(7), create_mask(8), NotifyObservers::No);

        // Time 800: single mask with 25 pixels (total area = 25, ABOVE threshold)
        mask_data->addAtTime(TimeFrameIndex(8), create_mask(25), NotifyObservers::No);

        // Time 900: single mask with 4 pixels (total area = 4, below threshold)
        mask_data->addAtTime(TimeFrameIndex(9), create_mask(4), NotifyObservers::No);

        m_data_manager->setData<MaskData>("input_masks", mask_data, TimeKey("default"));
    }

    std::unique_ptr<DataManager> m_data_manager;
    std::shared_ptr<TimeFrame> m_time_frame;
};

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE_METHOD(MaskToIntervalPipelineFixture,
                 "V2 Pipeline Integration: MaskData → Area → Sum → IntervalThreshold",
                 "[transforms][v2][integration][pipeline]") {

    DataManager* dm = getDataManager();

    SECTION("Execute full pipeline via JSON configuration") {
        // Create the multi-step pipeline JSON config
        // Step 1: Calculate area of each mask
        // Step 2: Sum all areas at each time point
        // Step 3: Threshold the summed areas to detect intervals
        nlohmann::json json_config = {
            {"metadata", {
                {"name", "Mask to Interval Pipeline"},
                {"description", "Detects intervals where total mask area exceeds threshold"},
                {"version", "1.0"}
            }},
            {"steps", {
                // Step 1: MaskData → RaggedAnalogTimeSeries (area per mask)
                {
                    {"step_id", "calculate_areas"},
                    {"transform_name", "CalculateMaskArea"},
                    {"input_key", "input_masks"},
                    {"output_key", "mask_areas"},
                    {"parameters", {
                        {"scale_factor", 1.0}
                    }}
                },
                // Step 2: RaggedAnalogTimeSeries → AnalogTimeSeries (sum at each time)
                {
                    {"step_id", "sum_areas"},
                    {"transform_name", "SumReduction"},
                    {"input_key", "mask_areas"},
                    {"output_key", "total_areas"},
                    {"parameters", {
                        {"ignore_nan", true},
                        {"default_value", 0.0}
                    }}
                },
                // Step 3: AnalogTimeSeries → DigitalIntervalSeries (threshold crossings)
                {
                    {"step_id", "detect_intervals"},
                    {"transform_name", "AnalogIntervalThreshold"},
                    {"input_key", "total_areas"},
                    {"output_key", "detected_intervals"},
                    {"parameters", {
                        {"threshold_value", 10.0},
                        {"direction", "positive"},
                        {"lockout_time", 0.0},
                        {"min_duration", 0.0},
                        {"missing_data_mode", "ignore"}
                    }}
                }
            }}
        };

        // Execute the pipeline
        DataManagerPipelineExecutor executor(dm);
        REQUIRE(executor.loadFromJson(json_config));

        // Validate before execution
        auto errors = executor.validate();
        REQUIRE(errors.empty());

        auto result = executor.execute();
        
        // Check execution success
        INFO("Pipeline error: " << result.error_message);
        REQUIRE(result.success);
        REQUIRE(result.steps_completed == 3);

        // Verify intermediate result: mask_areas (RaggedAnalogTimeSeries)
        SECTION("Verify intermediate: mask areas") {
            auto mask_areas = dm->getData<RaggedAnalogTimeSeries>("mask_areas");
            REQUIRE(mask_areas != nullptr);
            
            // Check that we have data at expected time points
            REQUIRE(mask_areas->getNumTimePoints() == 10);  // 10 time points
        }

        // Verify intermediate result: total_areas (AnalogTimeSeries)
        SECTION("Verify intermediate: summed areas") {
            auto total_areas = dm->getData<AnalogTimeSeries>("total_areas");
            REQUIRE(total_areas != nullptr);
            
            // Check expected summed values
            auto values = total_areas->getAnalogTimeSeries();
            REQUIRE(values.size() == 10);
            
            // Verify specific values
            REQUIRE_THAT(values[0], Catch::Matchers::WithinAbs(5.0, 0.01));   // Time 0
            REQUIRE_THAT(values[1], Catch::Matchers::WithinAbs(8.0, 0.01));   // Time 100
            REQUIRE_THAT(values[2], Catch::Matchers::WithinAbs(15.0, 0.01));  // Time 200
            REQUIRE_THAT(values[3], Catch::Matchers::WithinAbs(20.0, 0.01));  // Time 300
            REQUIRE_THAT(values[4], Catch::Matchers::WithinAbs(12.0, 0.01));  // Time 400
            REQUIRE_THAT(values[5], Catch::Matchers::WithinAbs(6.0, 0.01));   // Time 500
            REQUIRE_THAT(values[6], Catch::Matchers::WithinAbs(3.0, 0.01));   // Time 600
            REQUIRE_THAT(values[7], Catch::Matchers::WithinAbs(18.0, 0.01));  // Time 700
            REQUIRE_THAT(values[8], Catch::Matchers::WithinAbs(25.0, 0.01));  // Time 800
            REQUIRE_THAT(values[9], Catch::Matchers::WithinAbs(4.0, 0.01));   // Time 900
        }

        // Verify final result: detected_intervals (DigitalIntervalSeries)
        SECTION("Verify final: detected intervals") {
            auto intervals = dm->getData<DigitalIntervalSeries>("detected_intervals");
            REQUIRE(intervals != nullptr);
            
            auto const& interval_list = intervals->getDigitalIntervalSeries();
            
            // Should have 2 intervals where area > 10:
            // Interval 1: indices 2-4 (times 200-400)
            // Interval 2: indices 7-8 (times 700-800)
            // Note: Intervals use TimeFrameIndex values, not absolute time
            REQUIRE(interval_list.size() == 2);
            
            // First interval: index 2 to index 4
            REQUIRE(interval_list[0].start == 2);
            REQUIRE(interval_list[0].end == 4);
            
            // Second interval: index 7 to index 8
            REQUIRE(interval_list[1].start == 7);
            REQUIRE(interval_list[1].end == 8);
        }
    }

    SECTION("Execute pipeline with different threshold") {
        // Use a higher threshold so only the largest area periods are detected
        nlohmann::json json_config = {
            {"steps", {
                {
                    {"step_id", "calculate_areas"},
                    {"transform_name", "CalculateMaskArea"},
                    {"input_key", "input_masks"},
                    {"output_key", "mask_areas_v2"},
                    {"parameters", {}}
                },
                {
                    {"step_id", "sum_areas"},
                    {"transform_name", "SumReduction"},
                    {"input_key", "mask_areas_v2"},
                    {"output_key", "total_areas_v2"},
                    {"parameters", {}}
                },
                {
                    {"step_id", "detect_intervals"},
                    {"transform_name", "AnalogIntervalThreshold"},
                    {"input_key", "total_areas_v2"},
                    {"output_key", "detected_intervals_v2"},
                    {"parameters", {
                        {"threshold_value", 15.0},  // Higher threshold
                        {"direction", "positive"}
                    }}
                }
            }}
        };

        DataManagerPipelineExecutor executor(dm);
        REQUIRE(executor.loadFromJson(json_config));

        auto result = executor.execute();
        INFO("Pipeline error: " << result.error_message);
        REQUIRE(result.success);

        auto intervals = dm->getData<DigitalIntervalSeries>("detected_intervals_v2");
        REQUIRE(intervals != nullptr);
        
        auto const& interval_list = intervals->getDigitalIntervalSeries();
        
        // With threshold=15, only values > 15 are included (strictly greater):
        // Time 200 (idx 2): 15 (at threshold, NOT included since 15 > 15 is false)
        // Time 300 (idx 3): 20 (above, included)
        // Time 700 (idx 7): 18 (above, included)
        // Time 800 (idx 8): 25 (above, included)
        // So intervals: [3] (just index 3) and [7-8]
        REQUIRE(interval_list.size() == 2);
        
        REQUIRE(interval_list[0].start == 3);
        REQUIRE(interval_list[0].end == 3);
        
        REQUIRE(interval_list[1].start == 7);
        REQUIRE(interval_list[1].end == 8);
    }

    SECTION("Execute pipeline with min_duration filter") {
        // Require minimum duration of 3 samples (index span)
        nlohmann::json json_config = {
            {"steps", {
                {
                    {"step_id", "calculate_areas"},
                    {"transform_name", "CalculateMaskArea"},
                    {"input_key", "input_masks"},
                    {"output_key", "mask_areas_v3"},
                    {"parameters", {}}
                },
                {
                    {"step_id", "sum_areas"},
                    {"transform_name", "SumReduction"},
                    {"input_key", "mask_areas_v3"},
                    {"output_key", "total_areas_v3"},
                    {"parameters", {}}
                },
                {
                    {"step_id", "detect_intervals"},
                    {"transform_name", "AnalogIntervalThreshold"},
                    {"input_key", "total_areas_v3"},
                    {"output_key", "detected_intervals_v3"},
                    {"parameters", {
                        {"threshold_value", 10.0},
                        {"direction", "positive"},
                        {"min_duration", 3.0}  // Require at least 3 samples (index span)
                    }}
                }
            }}
        };

        DataManagerPipelineExecutor executor(dm);
        REQUIRE(executor.loadFromJson(json_config));

        auto result = executor.execute();
        INFO("Pipeline error: " << result.error_message);
        REQUIRE(result.success);

        auto intervals = dm->getData<DigitalIntervalSeries>("detected_intervals_v3");
        REQUIRE(intervals != nullptr);
        
        auto const& interval_list = intervals->getDigitalIntervalSeries();
        
        // min_duration uses index difference: end - start + 1
        // First interval [2-4]: 4 - 2 + 1 = 3 indices
        // Second interval [7-8]: 8 - 7 + 1 = 2 indices
        // With min_duration=3, only first interval passes
        REQUIRE(interval_list.size() == 1);
        REQUIRE(interval_list[0].start == 2);
        REQUIRE(interval_list[0].end == 4);
    }
}

TEST_CASE_METHOD(MaskToIntervalPipelineFixture,
                 "V2 Pipeline Integration: Verify step-by-step execution",
                 "[transforms][v2][integration][pipeline][detailed]") {

    DataManager* dm = getDataManager();

    // Execute just the first two steps to verify intermediate results
    nlohmann::json json_config = {
        {"steps", {
            {
                {"step_id", "step1_area"},
                {"transform_name", "CalculateMaskArea"},
                {"input_key", "input_masks"},
                {"output_key", "step1_areas"},
                {"parameters", {}}
            },
            {
                {"step_id", "step2_sum"},
                {"transform_name", "SumReduction"},
                {"input_key", "step1_areas"},
                {"output_key", "step2_summed"},
                {"parameters", {}}
            }
        }}
    };

    DataManagerPipelineExecutor executor(dm);
    REQUIRE(executor.loadFromJson(json_config));

    auto result = executor.execute();
    INFO("Pipeline error: " << result.error_message);
    REQUIRE(result.success);
    REQUIRE(result.steps_completed == 2);

    // Verify the RaggedAnalogTimeSeries has correct structure
    auto ragged_areas = dm->getData<RaggedAnalogTimeSeries>("step1_areas");
    REQUIRE(ragged_areas != nullptr);

    // Check ragged structure: some time points have multiple values
    // Time 100, 200, 400, 700 have multiple masks
    
    // Time 0: 1 mask
    auto data_t0 = ragged_areas->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(data_t0.size() == 1);
    REQUIRE_THAT(data_t0[0], Catch::Matchers::WithinAbs(5.0, 0.01));
    
    // Time 100 (index 1): 2 masks
    auto data_t1 = ragged_areas->getDataAtTime(TimeFrameIndex(1));
    REQUIRE(data_t1.size() == 2);
    
    // Time 200 (index 2): 2 masks
    auto data_t2 = ragged_areas->getDataAtTime(TimeFrameIndex(2));
    REQUIRE(data_t2.size() == 2);
    
    // Time 300 (index 3): 1 mask
    auto data_t3 = ragged_areas->getDataAtTime(TimeFrameIndex(3));
    REQUIRE(data_t3.size() == 1);
    REQUIRE_THAT(data_t3[0], Catch::Matchers::WithinAbs(20.0, 0.01));

    // Verify the AnalogTimeSeries has correct values
    auto summed = dm->getData<AnalogTimeSeries>("step2_summed");
    REQUIRE(summed != nullptr);
    REQUIRE(summed->getAnalogTimeSeries().size() == 10);
}
