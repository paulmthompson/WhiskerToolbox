/**
 * @file RemoveLineOutliers.test.cpp
 * @brief Tests for V2 element-level geometric outlier removal transform.
 */

#include "RemoveLineOutliers.hpp"

#include "Lines/Line_Data.hpp"
#include "TransformsV2/algorithms/LineResample/LineResample.hpp"
#include "TransformsV2/algorithms/MaskToLine/MaskToLine.hpp"
#include "TransformsV2/algorithms/MaskSkeletonize/MaskSkeletonize.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/io/ParameterIO.hpp"
#include "transforms/Masks/Mask_To_Line/mask_to_line.hpp"

#include "fixtures/scenarios/mask/mask_to_line_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;

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

Line2D orderedLineFromScenario(std::shared_ptr<MaskData> const & mask_data) {
    auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));
    auto const skeleton = skeletonizeMask(input_mask, skeletonizeParamsFromMaskData(*mask_data));
    return maskToLine(skeleton, MaskToLineParams{});
}

}// namespace

// ============================================================================
// Tests: RemoveLineOutliersParams JSON Validation
// ============================================================================

TEST_CASE("V2 RemoveLineOutliersParams - JSON Validation",
          "[transforms][v2][params][json][remove_line_outliers]") {

    SECTION("Accept empty JSON with defaults") {
        std::string const json = R"({})";
        auto const params = loadParametersFromJson<RemoveLineOutliersParams>(json);
        REQUIRE(params);
        REQUIRE(params.value().error_threshold == 5.0f);
        REQUIRE(params.value().polynomial_order == 3);
    }

    SECTION("Accept explicit parameters") {
        std::string const json = R"({"error_threshold": 7.5, "polynomial_order": 4})";
        auto const params = loadParametersFromJson<RemoveLineOutliersParams>(json);
        REQUIRE(params);
        REQUIRE(params.value().error_threshold == 7.5f);
        REQUIRE(params.value().polynomial_order == 4);
    }
}

// ============================================================================
// Core Functionality Tests
// ============================================================================

TEST_CASE("V2 RemoveLineOutliers - Empty and short lines unchanged",
          "[transforms][v2][remove_line_outliers][edge]") {
    RemoveLineOutliersParams const params;

    SECTION("Empty line") {
        Line2D const empty;
        auto const result = removeLineOutliers(empty, params);
        REQUIRE(result.empty());
    }

    SECTION("Too few points for filtering") {
        Line2D short_line = {{10.0f, 10.0f}, {11.0f, 10.0f}, {12.0f, 10.0f}};
        auto const result = removeLineOutliers(short_line, params);
        REQUIRE(result.size() == short_line.size());
    }
}

TEST_CASE("V2 RemoveLineOutliers - Filters ordered skeleton line via scenario",
          "[transforms][v2][remove_line_outliers][scenario]") {
    auto const mask_data = mask_to_line_scenarios::thin_rectangle();
    auto const ordered = orderedLineFromScenario(mask_data);
    REQUIRE_FALSE(ordered.empty());

    RemoveLineOutliersParams params;
    params.error_threshold = 5.0f;
    params.polynomial_order = 3;

    auto const filtered = removeLineOutliers(ordered, params);
    REQUIRE_FALSE(filtered.empty());
    REQUIRE(filtered.size() <= ordered.size());
}

TEST_CASE("V2 RemoveLineOutliers - validate() clamps polynomial order",
          "[transforms][v2][remove_line_outliers]") {
    RemoveLineOutliersParams params;
    params.polynomial_order = 15;
    params.validate();
    REQUIRE(params.polynomial_order == 9);
}

TEST_CASE("V2 RemoveLineOutliers - With Context",
          "[transforms][v2][remove_line_outliers][context]") {
    auto const mask_data = mask_to_line_scenarios::thin_rectangle();
    auto const ordered = orderedLineFromScenario(mask_data);
    RemoveLineOutliersParams const params;

    ComputeContext ctx;
    ctx.progress = [](int) {};
    ctx.is_cancelled = []() { return false; };

    auto const result = removeLineOutliersWithContext(ordered, params, ctx);
    REQUIRE_FALSE(result.empty());
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 RemoveLineOutliers - Registry Integration",
          "[transforms][v2][registry][remove_line_outliers]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered") {
        REQUIRE(registry.hasTransform("RemoveLineOutliers"));
    }

    SECTION("Context-aware transform is registered") {
        REQUIRE(registry.hasTransform("RemoveLineOutliersWithContext"));
    }

    SECTION("Execute via registry") {
        auto const mask_data = mask_to_line_scenarios::thin_rectangle();
        auto const ordered = orderedLineFromScenario(mask_data);
        RemoveLineOutliersParams const params;

        auto const result = registry.execute<Line2D, Line2D, RemoveLineOutliersParams>(
                "RemoveLineOutliers", ordered, params);

        REQUIRE_FALSE(result.empty());
    }
}

// ============================================================================
// Tests: Pipeline on LineData Container
// ============================================================================

TEST_CASE("V2 Pipeline: RemoveLineOutliers on LineData Container",
          "[transforms][v2][pipeline][remove_line_outliers]") {
    auto const mask_data = mask_to_line_scenarios::simple_rectangle();
    auto const ordered = orderedLineFromScenario(mask_data);
    REQUIRE_FALSE(ordered.empty());

    auto line_data = std::make_shared<LineData>();
    line_data->addAtTime(TimeFrameIndex(100), ordered, NotifyObservers::No);

    TransformPipeline pipeline;
    pipeline.addStep("RemoveLineOutliers", RemoveLineOutliersParams{});

    auto const view = pipeline.executeAsViewTyped<LineData, Line2D>(*line_data);

    size_t count = 0;
    for (auto const & [time, line]: view) {
        REQUIRE(time == TimeFrameIndex(100));
        REQUIRE_FALSE(line.empty());
        ++count;
    }
    REQUIRE(count == 1);
}

// ============================================================================
// Tests: V1 vs V2 Parity (outlier step in chain)
// ============================================================================

TEST_CASE("V2 RemoveLineOutliers - V1 parity in full chain",
          "[transforms][v2][remove_line_outliers][parity]") {
    MaskToLineParameters v1_params;
    v1_params.method = LinePointSelectionMethod::Skeletonize;
    v1_params.polynomial_order = 3;
    v1_params.error_threshold = 5.0f;
    v1_params.remove_outliers = true;
    v1_params.input_point_subsample_factor = 1;
    v1_params.should_smooth_line = false;
    v1_params.output_resolution = 5.0f;

    auto const mask_data = mask_to_line_scenarios::simple_rectangle();
    auto const v1_result = mask_to_line(mask_data.get(), &v1_params);
    REQUIRE(v1_result != nullptr);

    auto const ordered = orderedLineFromScenario(mask_data);
    RemoveLineOutliersParams outlier_params;
    LineResampleParams resample_params;
    resample_params.method = LineResampleMethod::FixedSpacing;
    resample_params.target_spacing = 5.0f;

    auto const v2_line = resampleLine(removeLineOutliers(ordered, outlier_params), resample_params);
    auto const v1_line = getFirstLineAt(*v1_result, TimeFrameIndex(100));

    REQUIRE_FALSE(v2_line.empty());
    REQUIRE(linesApproxEqual(v1_line, v2_line));
}
