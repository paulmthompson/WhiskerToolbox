/**
 * @file MaskToLine.test.cpp
 * @brief Tests for V2 element-level mask-to-line transform.
 */

#include "MaskToLine.hpp"

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "TransformsV2/algorithms/LineResample/LineResample.hpp"
#include "TransformsV2/algorithms/MaskSkeletonize/MaskSkeletonize.hpp"
#include "TransformsV2/algorithms/RemoveLineOutliers/RemoveLineOutliers.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/io/ParameterIO.hpp"
#include "transforms/Masks/Mask_To_Line/mask_to_line.hpp"

#include "fixtures/pipeline/pipeline_json_test_helpers.hpp"
#include "fixtures/scenarios/mask/mask_to_line_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstddef>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;
using namespace pipeline_json_test;
using Catch::Matchers::WithinAbs;

namespace {

MaskSkeletonizeParams skeletonizeParamsFromMaskData(MaskData const & mask_data) {
    MaskSkeletonizeParams params;
    ImageSize const image_size = mask_data.getImageSize();
    if (image_size.isDefined()) {
        params.image_width = image_size.width;
        params.image_height = image_size.height;
    }
    return params;
}

Mask2D getFirstMaskAt(MaskData const & mask_data, TimeFrameIndex time) {
    auto const masks = mask_data.getAtTime(time);
    REQUIRE(!masks.empty());
    return masks[0];
}

Line2D getFirstLineAt(LineData const & line_data, TimeFrameIndex time) {
    auto const lines = line_data.getAtTime(time);
    for (auto const & line: lines) {
        return line;
    }
    return Line2D{};
}

bool linesApproxEqual(Line2D const & a, Line2D const & b, float tolerance = 0.5f) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::abs(a[i].x - b[i].x) > tolerance || std::abs(a[i].y - b[i].y) > tolerance) {
            return false;
        }
    }
    return true;
}

Line2D runV2SkeletonizeChain(Mask2D const & mask, MaskData const & mask_data) {
    MaskSkeletonizeParams const skel_params = skeletonizeParamsFromMaskData(mask_data);
    MaskToLineParams const line_params;
    RemoveLineOutliersParams outlier_params;
    LineResampleParams resample_params;
    resample_params.method = LineResampleMethod::FixedSpacing;
    resample_params.target_spacing = 5.0f;

    auto line = maskToLine(skeletonizeMask(mask, skel_params), line_params);
    line = removeLineOutliers(line, outlier_params);
    return resampleLine(line, resample_params);
}

}// namespace

// ============================================================================
// Tests: MaskToLineParams JSON Validation
// ============================================================================

TEST_CASE("V2 MaskToLineParams - JSON Validation",
          "[transforms][v2][params][json][mask_to_line]") {

    SECTION("Accept empty JSON with defaults") {
        std::string const json = R"({})";
        auto const params = loadParametersFromJson<MaskToLineParams>(json);
        REQUIRE(params);
        REQUIRE(params.value().reference_x == 0.0f);
        REQUIRE(params.value().reference_y == 0.0f);
        REQUIRE(params.value().subsample_factor == 1);
    }

    SECTION("Accept explicit parameters") {
        std::string const json = R"({
            "reference_x": 5.0,
            "reference_y": 10.0,
            "subsample_factor": 2
        })";
        auto const params = loadParametersFromJson<MaskToLineParams>(json);
        REQUIRE(params);
        REQUIRE(params.value().reference_x == 5.0f);
        REQUIRE(params.value().reference_y == 10.0f);
        REQUIRE(params.value().subsample_factor == 2);
    }

    SECTION("Reject malformed JSON") {
        std::string const json = R"({"reference_x": 0.0, "invalid)";
        REQUIRE_FALSE(loadParametersFromJson<MaskToLineParams>(json));
    }
}

// ============================================================================
// Core Functionality Tests
// ============================================================================

TEST_CASE("V2 MaskToLine - Empty mask element",
          "[transforms][v2][mask_to_line][edge]") {
    Mask2D const empty_mask;
    MaskToLineParams const params;
    auto const result = maskToLine(empty_mask, params);
    REQUIRE(result.empty());
}

TEST_CASE("V2 MaskToLine - L-shaped mask with reference point via scenario",
          "[transforms][v2][mask_to_line][scenario]") {
    auto const mask_data = mask_to_line_scenarios::l_shaped_mask();
    auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));

    MaskToLineParams params;
    params.reference_x = 5.0f;
    params.reference_y = 5.0f;

    auto const result = maskToLine(input_mask, params);
    REQUIRE_FALSE(result.empty());
    REQUIRE(result.size() >= 2);
}

TEST_CASE("V2 MaskToLine - Subsample factor via scenario",
          "[transforms][v2][mask_to_line][scenario]") {
    auto const mask_data = mask_to_line_scenarios::subsample_test_mask();
    auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));

    MaskToLineParams params;
    params.subsample_factor = 5;

    auto const full = maskToLine(input_mask, MaskToLineParams{});
    auto const subsampled = maskToLine(input_mask, params);

    REQUIRE_FALSE(full.empty());
    REQUIRE_FALSE(subsampled.empty());
    REQUIRE(subsampled.size() <= full.size());
}

TEST_CASE("V2 MaskToLine - With Context",
          "[transforms][v2][mask_to_line][context]") {
    auto const mask_data = mask_to_line_scenarios::thin_rectangle();
    auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));
    MaskToLineParams const params;

    SECTION("Normal execution reports progress") {
        ComputeContext ctx;
        int progress_val = -1;
        ctx.progress = [&progress_val](int p) { progress_val = p; };
        ctx.is_cancelled = []() { return false; };

        auto const result = maskToLineWithContext(input_mask, params, ctx);
        REQUIRE_FALSE(result.empty());
        REQUIRE(progress_val == 100);
    }

    SECTION("Cancelled execution returns empty line") {
        ComputeContext ctx;
        ctx.progress = [](int) {};
        ctx.is_cancelled = []() { return true; };

        auto const result = maskToLineWithContext(input_mask, params, ctx);
        REQUIRE(result.empty());
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 MaskToLine - Registry Integration",
          "[transforms][v2][registry][mask_to_line]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered") {
        REQUIRE(registry.hasTransform("MaskToLine"));
    }

    SECTION("Context-aware transform is registered") {
        REQUIRE(registry.hasTransform("MaskToLineWithContext"));
    }

    SECTION("Can retrieve metadata") {
        auto const * metadata = registry.getMetadata("MaskToLine");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "MaskToLine");
        REQUIRE(metadata->category == "Image Processing");
    }

    SECTION("Execute via registry") {
        auto const mask_data = mask_to_line_scenarios::l_shaped_mask();
        auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));
        MaskToLineParams params;
        params.reference_x = 5.0f;
        params.reference_y = 5.0f;

        auto const result = registry.execute<Mask2D, Line2D, MaskToLineParams>(
                "MaskToLine", input_mask, params);

        REQUIRE_FALSE(result.empty());
    }
}

// ============================================================================
// Tests: Pipeline on MaskData Container
// ============================================================================

TEST_CASE("V2 Pipeline: MaskToLine on MaskData Container",
          "[transforms][v2][pipeline][mask_to_line]") {

    SECTION("Execute via pipeline on L-shaped mask") {
        auto mask_data = mask_to_line_scenarios::l_shaped_mask();
        auto const time_frame = std::make_shared<TimeFrame>();
        mask_data->setTimeFrame(time_frame);

        MaskToLineParams params;
        params.reference_x = 5.0f;
        params.reference_y = 5.0f;

        TransformPipeline pipeline;
        pipeline.addStep("MaskToLine", params);

        auto const view = pipeline.executeAsViewTyped<MaskData, Line2D>(*mask_data);

        size_t count = 0;
        for (auto const & [time, line]: view) {
            REQUIRE(time == TimeFrameIndex(100));
            REQUIRE_FALSE(line.empty());
            ++count;
        }
        REQUIRE(count == 1);
    }

    SECTION("Execute multi-step skeletonize chain on MaskData") {
        auto mask_data = mask_to_line_scenarios::simple_rectangle();
        auto const time_frame = std::make_shared<TimeFrame>();
        mask_data->setTimeFrame(time_frame);

        TransformPipeline pipeline;
        pipeline.addStep("SkeletonizeMask", skeletonizeParamsFromMaskData(*mask_data));
        pipeline.addStep("MaskToLine", MaskToLineParams{});

        auto const result = pipeline.executeOptimized<MaskData, LineData>(*mask_data);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTotalEntryCount() > 0);
    }

    SECTION("Multiple time frames") {
        auto mask_data = mask_to_line_scenarios::multiple_time_frames();
        auto const time_frame = std::make_shared<TimeFrame>();
        mask_data->setTimeFrame(time_frame);

        TransformPipeline pipeline;
        pipeline.addStep("SkeletonizeMask", skeletonizeParamsFromMaskData(*mask_data));
        pipeline.addStep("MaskToLine", MaskToLineParams{});

        auto const view = pipeline.executeAsViewTyped<MaskData, Line2D>(*mask_data);

        std::vector<TimeFrameIndex> times;
        for (auto const & [time, line]: view) {
            times.push_back(time);
            REQUIRE_FALSE(line.empty());
        }
        REQUIRE(times.size() == 2);
    }
}

// ============================================================================
// Tests: V1 vs V2 Parity
// ============================================================================

TEST_CASE("V2 MaskToLine - V1 parity via skeletonize chain",
          "[transforms][v2][mask_to_line][parity]") {

    MaskToLineParameters v1_params;
    v1_params.method = LinePointSelectionMethod::Skeletonize;
    v1_params.polynomial_order = 3;
    v1_params.error_threshold = 5.0f;
    v1_params.remove_outliers = true;
    v1_params.input_point_subsample_factor = 1;
    v1_params.should_smooth_line = false;
    v1_params.output_resolution = 5.0f;

    SECTION("Simple rectangular mask") {
        auto const mask_data = mask_to_line_scenarios::simple_rectangle();
        auto const v1_result = mask_to_line(mask_data.get(), &v1_params);
        REQUIRE(v1_result != nullptr);

        auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));
        auto const v2_line = runV2SkeletonizeChain(input_mask, *mask_data);
        auto const v1_line = getFirstLineAt(*v1_result, TimeFrameIndex(100));

        REQUIRE_FALSE(v2_line.empty());
        REQUIRE(linesApproxEqual(v1_line, v2_line));
    }

    SECTION("Thin rectangle mask") {
        auto const mask_data = mask_to_line_scenarios::thin_rectangle();
        auto const v1_result = mask_to_line(mask_data.get(), &v1_params);
        REQUIRE(v1_result != nullptr);

        auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));
        auto const v2_line = runV2SkeletonizeChain(input_mask, *mask_data);
        auto const v1_line = getFirstLineAt(*v1_result, TimeFrameIndex(100));

        REQUIRE_FALSE(v2_line.empty());
        REQUIRE(linesApproxEqual(v1_line, v2_line));
    }

    SECTION("Full container pipeline parity") {
        auto mask_data = mask_to_line_scenarios::json_pipeline_mask();
        auto const time_frame = std::make_shared<TimeFrame>();
        mask_data->setTimeFrame(time_frame);

        auto const v1_result = mask_to_line(mask_data.get(), &v1_params);
        REQUIRE(v1_result != nullptr);

        TransformPipeline pipeline;
        pipeline.addStep("SkeletonizeMask", skeletonizeParamsFromMaskData(*mask_data));
        pipeline.addStep("MaskToLine", MaskToLineParams{});
        RemoveLineOutliersParams outlier_params;
        pipeline.addStep("RemoveLineOutliers", outlier_params);
        LineResampleParams resample_params;
        resample_params.method = LineResampleMethod::FixedSpacing;
        resample_params.target_spacing = 5.0f;
        pipeline.addStep("ResampleLine", resample_params);

        auto const v2_result = pipeline.executeOptimized<MaskData, LineData>(*mask_data);
        REQUIRE(v2_result != nullptr);

        auto const v1_line = getFirstLineAt(*v1_result, TimeFrameIndex(100));
        auto const v2_line = getFirstLineAt(*v2_result, TimeFrameIndex(100));

        REQUIRE_FALSE(v1_line.empty());
        REQUIRE(linesApproxEqual(v1_line, v2_line));
    }
}

TEST_CASE("V2 MaskToLine - V1 parity nearest-to-reference chain",
          "[transforms][v2][mask_to_line][parity]") {

    MaskToLineParameters v1_params;
    v1_params.method = LinePointSelectionMethod::NearestToReference;
    v1_params.reference_x = 5.0f;
    v1_params.reference_y = 5.0f;
    v1_params.polynomial_order = 3;
    v1_params.error_threshold = 5.0f;
    v1_params.remove_outliers = true;
    v1_params.input_point_subsample_factor = 1;
    v1_params.should_smooth_line = false;
    v1_params.output_resolution = 5.0f;

    auto const mask_data = mask_to_line_scenarios::l_shaped_mask();
    auto const v1_result = mask_to_line(mask_data.get(), &v1_params);
    REQUIRE(v1_result != nullptr);

    MaskToLineParams line_params;
    line_params.reference_x = 5.0f;
    line_params.reference_y = 5.0f;

    TransformPipeline pipeline;
    pipeline.addStep("MaskToLine", line_params);
    pipeline.addStep("RemoveLineOutliers", RemoveLineOutliersParams{});
    LineResampleParams resample_params;
    resample_params.method = LineResampleMethod::FixedSpacing;
    resample_params.target_spacing = 5.0f;
    pipeline.addStep("ResampleLine", resample_params);

    auto const v2_result = pipeline.executeOptimized<MaskData, LineData>(*mask_data);
    REQUIRE(v2_result != nullptr);

    auto const v1_line = getFirstLineAt(*v1_result, TimeFrameIndex(100));
    auto const v2_line = getFirstLineAt(*v2_result, TimeFrameIndex(100));

    REQUIRE_FALSE(v1_line.empty());
    REQUIRE(linesApproxEqual(v1_line, v2_line));
}

// ============================================================================
// Tests: DataManager Integration
// ============================================================================

TEST_CASE("V2 DataManager Integration: MaskToLine via executor",
          "[transforms][v2][datamanager][mask_to_line]") {
    DataManager dm;
    auto const time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    auto const mask_data = mask_to_line_scenarios::json_pipeline_mask();
    mask_data->setTimeFrame(time_frame);
    dm.setData("test_mask", mask_data, TimeKey("default"));

    MaskSkeletonizeParams const skel_params = skeletonizeParamsFromMaskData(*mask_data);
    MaskToLineParams const line_params;

    auto const pipeline = makePipeline({
            makeStep("1", "SkeletonizeMask", "test_mask", "skeletonized_mask", skel_params),
            makeStep("2", "MaskToLine", "skeletonized_mask", "converted_line", line_params),
    });

    auto const result = executeViaExecutor(dm, pipeline);
    REQUIRE(result.load_ok);
    REQUIRE(result.execution.success);

    auto const line_data = dm.getData<LineData>("converted_line");
    REQUIRE(line_data != nullptr);
    REQUIRE(line_data->getTotalEntryCount() > 0);
}
