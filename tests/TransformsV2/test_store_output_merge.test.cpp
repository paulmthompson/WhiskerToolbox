/**
 * @file test_store_output_merge.test.cpp
 * @brief Tests for storeOutputData overwrite-merge when output_key already exists.
 */

#include "TransformsV2/algorithms/MaskArea/MaskArea.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"

#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "CoreGeometry/masks.hpp"
#include "DataManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/DataManagerMerge.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

namespace {

[[nodiscard]] Mask2D make_mask(int point_count) {
    std::vector<Point2D<uint32_t>> points;
    points.reserve(static_cast<size_t>(point_count));
    for (int i = 0; i < point_count; ++i) {
        points.emplace_back(static_cast<uint32_t>(i), 0u);
    }
    return Mask2D(points);
}

struct TestSetup {
    std::unique_ptr<DataManager> dm;
    std::string time_key_name;
};

TestSetup createTestData(std::string const & time_key_name = "camera_time") {
    auto dm = std::make_unique<DataManager>();

    std::vector<int> const times = {0, 100, 200, 300, 400};
    auto const time_frame = std::make_shared<TimeFrame>(times);
    dm->setTime(TimeKey(time_key_name), time_frame);

    auto mask_data = std::make_shared<MaskData>();
    mask_data->setTimeFrame(time_frame);

    for (int i = 0; i < 5; ++i) {
        mask_data->addAtTime(TimeFrameIndex(i), make_mask(i + 1), NotifyObservers::No);
    }

    dm->setData<MaskData>("input_masks", mask_data, TimeKey(time_key_name));

    return {std::move(dm), time_key_name};
}

}// namespace

TEST_CASE("Pipeline store path merges into existing MaskData output key",
          "[transforms][v2][executor][merge]") {
    auto dm = std::make_unique<DataManager>();
    auto const time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 100, 200, 300, 400});
    dm->setTime(TimeKey("camera_time"), time_frame);

    auto existing = std::make_shared<MaskData>();
    existing->setTimeFrame(time_frame);
    existing->addAtTime(TimeFrameIndex(0), make_mask(1), NotifyObservers::No);
    existing->addAtTime(TimeFrameIndex(100), make_mask(2), NotifyObservers::No);
    existing->addAtTime(TimeFrameIndex(200), make_mask(3), NotifyObservers::No);
    dm->setData<MaskData>("output_masks", existing, TimeKey("camera_time"));

    auto pipeline_output = std::make_shared<MaskData>();
    pipeline_output->setTimeFrame(time_frame);
    pipeline_output->addAtTime(TimeFrameIndex(100), make_mask(9), NotifyObservers::No);
    pipeline_output->addAtTime(TimeFrameIndex(400), make_mask(4), NotifyObservers::No);

    std::string error;
    auto const merged =
            mergeOverwriteData(*dm, "output_masks", DataTypeVariant{pipeline_output}, error);

    REQUIRE(merged.has_value());
    REQUIRE(error.empty());

    auto const result = dm->getData<MaskData>("output_masks");
    REQUIRE(result.get() == existing.get());
    REQUIRE(result->getAtTime(TimeFrameIndex(0)).size() == 1);
    REQUIRE(result->getAtTime(TimeFrameIndex(100)).size() == 1);
    REQUIRE(result->getAtTime(TimeFrameIndex(200)).size() == 1);
    REQUIRE(result->getAtTime(TimeFrameIndex(400)).size() == 1);
    REQUIRE((*result->getAtTime(TimeFrameIndex(100)).begin()).size() == 9);
}

TEST_CASE("executeStep replaces existing non-mergeable output key",
          "[transforms][v2][executor][merge]") {
    auto [dm, tk_name] = createTestData();
    auto const time_frame = dm->getTime(TimeKey(tk_name));

    auto const stale_output = std::make_shared<RaggedAnalogTimeSeries>();
    stale_output->setTimeFrame(time_frame);
    stale_output->setDataAtTime(
            TimeFrameIndex(0), std::vector<float>{99.0f}, NotifyObservers::No);
    dm->setData<RaggedAnalogTimeSeries>("mask_areas", stale_output, TimeKey(tk_name));
    auto const old_ptr = dm->getData<RaggedAnalogTimeSeries>("mask_areas");

    nlohmann::json const config = {
            {"steps",
             {{{"step_id", "areas"},
               {"transform_name", "CalculateMaskArea"},
               {"input_key", "input_masks"},
               {"output_key", "mask_areas"},
               {"parameters", {{"scale_factor", 1.0}}}}}}};

    DataManagerPipelineExecutor executor(dm.get());
    REQUIRE(executor.loadFromJson(config));

    auto const step_result = executor.executeStep(0);
    INFO("Error: " << step_result.error_message);
    REQUIRE(step_result.success);

    auto const new_ptr = dm->getData<RaggedAnalogTimeSeries>("mask_areas");
    REQUIRE(new_ptr.get() != old_ptr.get());
    REQUIRE(new_ptr->getNumTimePoints() == 5);
    REQUIRE(new_ptr->getValuesAtTimeVec(TimeFrameIndex(0))[0] == 1.0f);
}
