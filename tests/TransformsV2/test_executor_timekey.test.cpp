/**
 * @file test_executor_timekey.test.cpp
 * @brief Tests for DataManagerPipelineExecutor TimeKey propagation and output_time_key support.
 *
 * Verifies that:
 * - Steps without output_time_key propagate the input data's TimeKey (not hardcoded "default")
 * - Steps with output_time_key store output under the specified TimeKey
 * - Nonexistent output_time_key causes step failure with clear error
 * - Size mismatch between output and target TimeFrame causes step failure
 */

#include "TransformsV2/algorithms/MaskArea/MaskArea.hpp"
#include "TransformsV2/algorithms/SumReduction/SumReduction.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "CoreGeometry/masks.hpp"
#include "DataManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

namespace {

/// @brief Create a DataManager with MaskData under a non-default TimeKey.
/// Returns {DataManager, TimeKey name} for testing.
struct TestSetup {
    std::unique_ptr<DataManager> dm;
    std::string time_key_name;
};

TestSetup createTestData(std::string const & time_key_name = "camera_time") {
    auto dm = std::make_unique<DataManager>();

    // Create a TimeFrame with 5 entries: [0, 100, 200, 300, 400]
    std::vector<int> const times = {0, 100, 200, 300, 400};
    auto tf = std::make_shared<TimeFrame>(times);
    dm->setTime(TimeKey(time_key_name), tf);

    // Create MaskData with one mask per time point (area = index + 1)
    auto mask_data = std::make_shared<MaskData>();
    mask_data->setTimeFrame(tf);

    auto make_mask = [](int n) {
        std::vector<Point2D<uint32_t>> pts;
        pts.reserve(n);
        for (int i = 0; i < n; ++i) {
            pts.emplace_back(
                    static_cast<uint32_t>(i), 0u);
        }
        return Mask2D(pts);
    };

    for (int i = 0; i < 5; ++i) {
        mask_data->addAtTime(TimeFrameIndex(i), make_mask(i + 1), NotifyObservers::No);
    }

    dm->setData<MaskData>("input_masks", mask_data, TimeKey(time_key_name));

    return {std::move(dm), time_key_name};
}

}// anonymous namespace

// ============================================================================
// TimeKey Propagation Tests
// ============================================================================

TEST_CASE("Executor propagates input TimeKey when output_time_key is absent",
          "[transforms][v2][executor][timekey]") {
    auto [dm, tk_name] = createTestData("my_camera");

    // Pipeline: MaskArea → SumReduction (produces AnalogTimeSeries)
    nlohmann::json const config = {
            {"steps", {{{"step_id", "areas"}, {"transform_name", "CalculateMaskArea"}, {"input_key", "input_masks"}, {"output_key", "mask_areas"}, {"parameters", {{"scale_factor", 1.0}}}}, {{"step_id", "sums"}, {"transform_name", "SumReduction"}, {"input_key", "mask_areas"}, {"output_key", "total_areas"}, {"parameters", {{"ignore_nan", true}, {"default_value", 0.0}}}}}}};

    DataManagerPipelineExecutor executor(dm.get());
    REQUIRE(executor.loadFromJson(config));

    auto result = executor.execute();
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);

    // The output should inherit the input's TimeKey ("my_camera"), not "default"
    TimeKey const output_tk = dm->getTimeKey("total_areas");
    REQUIRE(output_tk.str() == tk_name);
}

TEST_CASE("Executor stores output under output_time_key when specified",
          "[transforms][v2][executor][timekey]") {
    auto [dm, tk_name] = createTestData("camera_time");

    // Create a second TimeKey with the same size (5 entries) for the output
    std::vector<int> const upsampled_times = {0, 50, 100, 150, 200};
    auto upsampled_tf = std::make_shared<TimeFrame>(upsampled_times);
    dm->setTime(TimeKey("upsampled_time"), upsampled_tf);

    // Pipeline: MaskArea → SumReduction with output_time_key
    nlohmann::json const config = {
            {"steps", {{{"step_id", "areas"}, {"transform_name", "CalculateMaskArea"}, {"input_key", "input_masks"}, {"output_key", "mask_areas"}, {"parameters", {{"scale_factor", 1.0}}}}, {{"step_id", "sums"}, {"transform_name", "SumReduction"}, {"input_key", "mask_areas"}, {"output_key", "total_areas"}, {"output_time_key", "upsampled_time"}, {"parameters", {{"ignore_nan", true}, {"default_value", 0.0}}}}}}};

    DataManagerPipelineExecutor executor(dm.get());
    REQUIRE(executor.loadFromJson(config));

    auto result = executor.execute();
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);

    // The output should be stored under "upsampled_time"
    TimeKey const output_tk = dm->getTimeKey("total_areas");
    REQUIRE(output_tk.str() == "upsampled_time");
}

TEST_CASE("Executor fails when output_time_key does not exist in DataManager",
          "[transforms][v2][executor][timekey]") {
    auto [dm, tk_name] = createTestData();

    nlohmann::json const config = {
            {"steps", {{{"step_id", "areas"}, {"transform_name", "CalculateMaskArea"}, {"input_key", "input_masks"}, {"output_key", "mask_areas"}, {"output_time_key", "nonexistent_timekey"}, {"parameters", {{"scale_factor", 1.0}}}}}}};

    DataManagerPipelineExecutor executor(dm.get());
    REQUIRE(executor.loadFromJson(config));

    auto result = executor.execute();
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("nonexistent_timekey") != std::string::npos);
    REQUIRE(result.error_message.find("does not exist") != std::string::npos);
}

TEST_CASE("Executor fails when output size does not match target TimeFrame",
          "[transforms][v2][executor][timekey]") {
    auto [dm, tk_name] = createTestData();

    // Create a TimeFrame with a DIFFERENT size (3 entries instead of 5)
    std::vector<int> const wrong_size_times = {0, 50, 100};
    auto wrong_tf = std::make_shared<TimeFrame>(wrong_size_times);
    dm->setTime(TimeKey("wrong_size_tf"), wrong_tf);

    // SumReduction will produce 5 samples (one per input time), but
    // the target TimeFrame has only 3 entries → size mismatch
    nlohmann::json const config = {
            {"steps", {{{"step_id", "areas"}, {"transform_name", "CalculateMaskArea"}, {"input_key", "input_masks"}, {"output_key", "mask_areas"}, {"parameters", {{"scale_factor", 1.0}}}}, {{"step_id", "sums"}, {"transform_name", "SumReduction"}, {"input_key", "mask_areas"}, {"output_key", "total_areas"}, {"output_time_key", "wrong_size_tf"}, {"parameters", {{"ignore_nan", true}, {"default_value", 0.0}}}}}}};

    DataManagerPipelineExecutor executor(dm.get());
    REQUIRE(executor.loadFromJson(config));

    auto result = executor.execute();
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find('5') != std::string::npos);
    REQUIRE(result.error_message.find('3') != std::string::npos);
    REQUIRE(result.error_message.find("wrong_size_tf") != std::string::npos);
}

TEST_CASE("Executor parses output_time_key from JSON descriptor",
          "[transforms][v2][executor][timekey]") {
    auto dm = std::make_unique<DataManager>();

    nlohmann::json const config = {
            {"steps", {{{"step_id", "s1"}, {"transform_name", "CalculateMaskArea"}, {"input_key", "in"}, {"output_key", "out"}, {"output_time_key", "my_time"}}}}};

    DataManagerPipelineExecutor executor(dm.get());
    REQUIRE(executor.loadFromJson(config));

    auto const & steps = executor.getSteps();
    REQUIRE(steps.size() == 1);
    REQUIRE(steps[0].output_time_key.has_value());
    REQUIRE(steps[0].output_time_key.value() == "my_time");
}

TEST_CASE("Executor omits output_time_key when not in JSON",
          "[transforms][v2][executor][timekey]") {
    auto dm = std::make_unique<DataManager>();

    nlohmann::json const config = {
            {"steps", {{{"step_id", "s1"}, {"transform_name", "CalculateMaskArea"}, {"input_key", "in"}, {"output_key", "out"}}}}};

    DataManagerPipelineExecutor executor(dm.get());
    REQUIRE(executor.loadFromJson(config));

    auto const & steps = executor.getSteps();
    REQUIRE(steps.size() == 1);
    REQUIRE_FALSE(steps[0].output_time_key.has_value());
}
