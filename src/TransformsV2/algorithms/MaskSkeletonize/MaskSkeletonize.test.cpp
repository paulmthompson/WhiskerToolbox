/**
 * @file MaskSkeletonize.test.cpp
 * @brief Tests for V2 element-level mask skeletonization transform.
 */

#include "MaskSkeletonize.hpp"

#include "DataManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/io/ParameterIO.hpp"

#include "fixtures/pipeline/pipeline_json_test_helpers.hpp"
#include "fixtures/scenarios/mask/skeletonize_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;
using namespace pipeline_json_test;

namespace {

MaskSkeletonizeParams paramsFromMaskData(MaskData const & mask_data) {
    MaskSkeletonizeParams params;
    ImageSize const image_size = mask_data.getImageSize();
    if (image_size.isDefined()) {
        params.image_width = image_size.width;
        params.image_height = image_size.height;
    }
    return params;
}

size_t countMaskPoints(MaskData const & mask_data, TimeFrameIndex time) {
    size_t total = 0;
    auto const masks = mask_data.getAtTime(time);
    for (auto const & mask: masks) {
        total += mask.size();
    }
    return total;
}

size_t countMaskPoints(Mask2D const & mask) {
    return mask.size();
}

Mask2D getFirstMaskAt(MaskData const & mask_data, TimeFrameIndex time) {
    auto const masks = mask_data.getAtTime(time);
    REQUIRE(!masks.empty());
    return masks[0];
}

}// namespace

// ============================================================================
// Tests: MaskSkeletonizeParams JSON Validation
// ============================================================================

TEST_CASE("V2 MaskSkeletonizeParams - JSON Validation",
          "[transforms][v2][params][json][mask_skeletonize]") {

    SECTION("Accept empty JSON") {
        std::string const json = R"({})";
        auto const params = loadParametersFromJson<MaskSkeletonizeParams>(json);
        REQUIRE(params);
        REQUIRE(params.value().image_width == -1);
        REQUIRE(params.value().image_height == -1);
    }

    SECTION("Accept explicit canvas dimensions") {
        std::string const json = R"({"image_width": 640, "image_height": 480})";
        auto const params = loadParametersFromJson<MaskSkeletonizeParams>(json);
        REQUIRE(params);
        REQUIRE(params.value().image_width == 640);
        REQUIRE(params.value().image_height == 480);
    }

    SECTION("Reject malformed JSON") {
        std::string const json = R"({
            "image_width": 640,
            "invalid
        })";
        REQUIRE_FALSE(loadParametersFromJson<MaskSkeletonizeParams>(json));
    }

    SECTION("Reject non-numeric image_width") {
        std::string const json = R"({"image_width": "wide"})";
        REQUIRE_FALSE(loadParametersFromJson<MaskSkeletonizeParams>(json));
    }
}

// ============================================================================
// Core Functionality Tests (using scenarios shared with V1)
// ============================================================================

TEST_CASE("V2 Mask Skeletonize - Simple rectangular mask via scenario",
          "[transforms][v2][mask_skeletonize][scenario]") {
    auto const mask_data = mask_scenarios::rectangular_mask_10x10();
    MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);

    auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));
    auto const result = skeletonizeMask(input_mask, params);

    REQUIRE_FALSE(result.empty());
    REQUIRE(countMaskPoints(result) < countMaskPoints(input_mask));
}

TEST_CASE("V2 Mask Skeletonize - Empty mask element via scenario",
          "[transforms][v2][mask_skeletonize][edge][scenario]") {
    auto const mask_data = mask_scenarios::empty_mask_data();
    MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);

    REQUIRE(mask_data->getTimesWithData().empty());

    Mask2D const empty_mask;
    auto const result = skeletonizeMask(empty_mask, params);
    REQUIRE(result.empty());
}

TEST_CASE("V2 Mask Skeletonize - Single point mask via scenario",
          "[transforms][v2][mask_skeletonize][edge][scenario]") {
    auto const mask_data = mask_scenarios::single_point_mask_skeletonize();
    MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);

    auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));
    auto const result = skeletonizeMask(input_mask, params);

    REQUIRE_FALSE(result.empty());
    REQUIRE(result.size() == 1);
}

TEST_CASE("V2 Mask Skeletonize - Multiple time frames via scenario",
          "[transforms][v2][mask_skeletonize][scenario]") {
    auto const mask_data = mask_scenarios::multi_frame_masks_skeletonize();
    MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);

    auto const input_100 = getFirstMaskAt(*mask_data, TimeFrameIndex(100));
    auto const input_105 = getFirstMaskAt(*mask_data, TimeFrameIndex(105));

    auto const result_100 = skeletonizeMask(input_100, params);
    auto const result_105 = skeletonizeMask(input_105, params);

    REQUIRE_FALSE(result_100.empty());
    REQUIRE_FALSE(result_105.empty());
}

// ============================================================================
// Tests: Context-aware version
// ============================================================================

TEST_CASE("V2 Mask Skeletonize - With Context",
          "[transforms][v2][mask_skeletonize][context]") {
    auto const mask_data = mask_scenarios::rectangular_mask_10x10();
    MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);
    auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));

    SECTION("Normal execution reports progress") {
        ComputeContext ctx;
        int progress_val = -1;
        ctx.progress = [&progress_val](int p) { progress_val = p; };
        ctx.is_cancelled = []() { return false; };

        auto const result = skeletonizeMaskWithContext(input_mask, params, ctx);

        REQUIRE_FALSE(result.empty());
        REQUIRE(progress_val == 100);
    }

    SECTION("Cancelled execution returns empty mask") {
        ComputeContext ctx;
        ctx.progress = [](int) {};
        ctx.is_cancelled = []() { return true; };

        auto const result = skeletonizeMaskWithContext(input_mask, params, ctx);
        REQUIRE(result.empty());
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Mask Skeletonize - Registry Integration",
          "[transforms][v2][registry][mask_skeletonize]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered") {
        REQUIRE(registry.hasTransform("SkeletonizeMask"));
    }

    SECTION("Context-aware transform is registered") {
        REQUIRE(registry.hasTransform("SkeletonizeMaskWithContext"));
    }

    SECTION("Can retrieve metadata") {
        auto const * metadata = registry.getMetadata("SkeletonizeMask");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "SkeletonizeMask");
        REQUIRE(metadata->category == "Image Processing");
        REQUIRE(metadata->is_expensive);
    }

    SECTION("Execute via registry") {
        auto const mask_data = mask_scenarios::rectangular_mask_10x10();
        MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);
        auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));

        auto const result = registry.execute<Mask2D, Mask2D, MaskSkeletonizeParams>(
                "SkeletonizeMask", input_mask, params);

        REQUIRE_FALSE(result.empty());
        REQUIRE(result.size() < input_mask.size());
    }
}

// ============================================================================
// Tests: Pipeline on MaskData Container
// ============================================================================

TEST_CASE("V2 Pipeline: SkeletonizeMask on MaskData Container",
          "[transforms][v2][pipeline][mask_skeletonize]") {

    SECTION("Execute via pipeline on single-frame MaskData") {
        auto mask_data = mask_scenarios::rectangular_mask_10x10();
        auto const time_frame = std::make_shared<TimeFrame>();
        mask_data->setTimeFrame(time_frame);

        MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);

        TransformPipeline pipeline;
        pipeline.addStep("SkeletonizeMask", params);

        auto const view = pipeline.executeAsViewTyped<MaskData, Mask2D>(*mask_data);

        size_t count = 0;
        for (auto const & [time, mask]: view) {
            REQUIRE(time == TimeFrameIndex(100));
            REQUIRE_FALSE(mask.empty());
            REQUIRE(mask.size() < getFirstMaskAt(*mask_data, time).size());
            ++count;
        }
        REQUIRE(count == 1);
    }

    SECTION("Execute via pipeline on multi-frame MaskData") {
        auto mask_data = mask_scenarios::multi_frame_masks_skeletonize();
        auto const time_frame = std::make_shared<TimeFrame>();
        mask_data->setTimeFrame(time_frame);

        MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);

        TransformPipeline pipeline;
        pipeline.addStep("SkeletonizeMask", params);

        auto const view = pipeline.executeAsViewTyped<MaskData, Mask2D>(*mask_data);

        std::vector<TimeFrameIndex> times;
        for (auto const & [time, mask]: view) {
            times.push_back(time);
            REQUIRE_FALSE(mask.empty());
        }

        REQUIRE(times.size() == 2);
        REQUIRE(times[0] == TimeFrameIndex(100));
        REQUIRE(times[1] == TimeFrameIndex(105));
    }

    SECTION("Materialize pipeline result into MaskData") {
        auto mask_data = mask_scenarios::json_pipeline_rectangular_skeletonize();
        auto const time_frame = std::make_shared<TimeFrame>();
        mask_data->setTimeFrame(time_frame);

        size_t const original_points = countMaskPoints(*mask_data, TimeFrameIndex(100));

        MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);

        TransformPipeline pipeline;
        pipeline.addStep("SkeletonizeMask", params);

        auto const view = pipeline.executeAsViewTyped<MaskData, Mask2D>(*mask_data);
        auto const result = std::make_shared<MaskData>(view);
        result->setTimeFrame(time_frame);

        REQUIRE(countMaskPoints(*result, TimeFrameIndex(100)) < original_points);
    }
}

// ============================================================================
// Tests: DataManager Integration
// ============================================================================

TEST_CASE("V2 DataManager Integration: SkeletonizeMask via executor",
          "[transforms][v2][datamanager][mask_skeletonize]") {
    DataManager dm;
    auto const time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    auto const mask_data = mask_scenarios::json_pipeline_rectangular_skeletonize();
    mask_data->setTimeFrame(time_frame);
    dm.setData("test_mask", mask_data, TimeKey("default"));

    size_t const original_points = countMaskPoints(*mask_data, TimeFrameIndex(100));

    MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);
    auto const pipeline = makeSingleStepPipeline(
            "SkeletonizeMask",
            "test_mask",
            "skeletonized_mask",
            params,
            "skeletonize_step");

    auto const result = executeViaExecutor(dm, pipeline);
    REQUIRE(result.load_ok);
    REQUIRE(result.execution.success);

    auto const skeletonized = dm.getData<MaskData>("skeletonized_mask");
    REQUIRE(skeletonized != nullptr);
    REQUIRE(countMaskPoints(*skeletonized, TimeFrameIndex(100)) < original_points);
}

TEST_CASE("V2 DataManager Integration: SkeletonizeMask via load_data_from_json_config_v2",
          "[transforms][v2][datamanager][json_config][mask_skeletonize]") {
    DataManager dm;
    auto const time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    auto const mask_data = mask_scenarios::multi_frame_masks_skeletonize();
    mask_data->setTimeFrame(time_frame);
    dm.setData("test_mask_multiframe", mask_data, TimeKey("default"));

    MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);
    auto const pipeline = makeSingleStepPipeline(
            "SkeletonizeMask",
            "test_mask_multiframe",
            "skeletonized_mask_multiframe",
            params,
            "skeletonize_multiframe_step");

    executeViaLoadDataFromJsonConfigV2(dm, pipeline);

    auto const skeletonized = dm.getData<MaskData>("skeletonized_mask_multiframe");
    REQUIRE(skeletonized != nullptr);

    REQUIRE_FALSE(skeletonized->getAtTime(TimeFrameIndex(100)).empty());
    REQUIRE_FALSE(skeletonized->getAtTime(TimeFrameIndex(105)).empty());
}

TEST_CASE("V2 DataManager Integration: SkeletonizeMask empty mask data via scenario",
          "[transforms][v2][datamanager][mask_skeletonize][edge]") {
    DataManager dm;
    auto const time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    auto const mask_data = mask_scenarios::empty_mask_data();
    dm.setData("empty_mask_data", mask_data, TimeKey("default"));

    MaskSkeletonizeParams const params = paramsFromMaskData(*mask_data);
    auto const pipeline = makeSingleStepPipeline(
            "SkeletonizeMask",
            "empty_mask_data",
            "skeletonized_empty",
            params,
            "empty_skeletonize_step");

    auto const result = executeViaExecutor(dm, pipeline);
    REQUIRE(result.load_ok);
    REQUIRE(result.execution.success);

    auto const skeletonized = dm.getData<MaskData>("skeletonized_empty");
    REQUIRE(skeletonized != nullptr);
    REQUIRE(skeletonized->getTimesWithData().empty());
}
