/**
 * @file MaskMedianFilter.test.cpp
 * @brief Tests for V2 element-level mask median filtering transform.
 */

#include "MaskMedianFilter.hpp"

#include "DataManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/io/ParameterIO.hpp"

#include "fixtures/pipeline/pipeline_json_test_helpers.hpp"
#include "fixtures/scenarios/mask/skeletonize_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;
using namespace pipeline_json_test;

namespace {

MaskMedianFilterParams paramsFromMaskData(MaskData const & mask_data) {
    MaskMedianFilterParams params;
    ImageSize const image_size = mask_data.getImageSize();
    if (image_size.isDefined()) {
        params.image_width = image_size.width;
        params.image_height = image_size.height;
    }
    return params;
}

size_t countMaskPoints(Mask2D const & mask) {
    return mask.size();
}

Mask2D getFirstMaskAt(MaskData const & mask_data, TimeFrameIndex time) {
    auto const masks = mask_data.getAtTime(time);
    REQUIRE(!masks.empty());
    return masks[0];
}

MaskMedianFilterParams tightCanvasParams() {
    return MaskMedianFilterParams{};
}

MaskMedianFilterParams explicitCanvasParams(int width, int height) {
    MaskMedianFilterParams params;
    params.image_width = width;
    params.image_height = height;
    return params;
}

bool maskContainsPoint(Mask2D const & mask, uint32_t x, uint32_t y) {
    for (auto const & point: mask) {
        if (point.x == x && point.y == y) {
            return true;
        }
    }
    return false;
}

Mask2D horizontalBar(uint32_t x0, uint32_t y0, uint32_t width, uint32_t height) {
    Mask2D mask;
    for (uint32_t y = y0; y < y0 + height; ++y) {
        for (uint32_t x = x0; x < x0 + width; ++x) {
            mask.push_back({x, y});
        }
    }
    return mask;
}

}// namespace

// ============================================================================
// Tests: MaskMedianFilterParams JSON Validation
// ============================================================================

TEST_CASE("V2 MaskMedianFilterParams - JSON Validation",
          "[transforms][v2][params][json][mask_median_filter]") {

    SECTION("Load empty JSON uses defaults") {
        std::string const json = R"({})";
        auto const params = loadParametersFromJson<MaskMedianFilterParams>(json);
        REQUIRE(params);
        REQUIRE(params.value().window_size == 3);
        REQUIRE(params.value().image_width == -1);
        REQUIRE(params.value().image_height == -1);
    }

    SECTION("Load window_size and canvas dimensions") {
        std::string const json = R"({
            "window_size": 5,
            "image_width": 640,
            "image_height": 480
        })";
        auto const params = loadParametersFromJson<MaskMedianFilterParams>(json);
        REQUIRE(params);
        REQUIRE(params.value().window_size == 5);
        REQUIRE(params.value().image_width == 640);
        REQUIRE(params.value().image_height == 480);
    }

    SECTION("Reject malformed JSON") {
        std::string const json = R"({
            "window_size": 3,
            "invalid
        })";
        REQUIRE_FALSE(loadParametersFromJson<MaskMedianFilterParams>(json));
    }

    SECTION("Reject non-numeric window_size") {
        std::string const json = R"({"window_size": "large"})";
        REQUIRE_FALSE(loadParametersFromJson<MaskMedianFilterParams>(json));
    }
}

// ============================================================================
// Core Functionality Tests
// ============================================================================

TEST_CASE("V2 Mask Median Filter - Removes isolated noise pixels",
          "[transforms][v2][mask_median_filter]") {
    Mask2D input = horizontalBar(10, 20, 9, 3);
    input.push_back({0, 0});
    input.push_back({50, 50});

    MaskMedianFilterParams params = tightCanvasParams();
    params.window_size = 3;

    auto const result = applyMedianFilter(input, params);

    REQUIRE_FALSE(result.empty());
    REQUIRE(countMaskPoints(result) < countMaskPoints(input));
    REQUIRE_FALSE(maskContainsPoint(result, 0, 0));
    REQUIRE_FALSE(maskContainsPoint(result, 50, 50));
    REQUIRE(maskContainsPoint(result, 14, 21));
}

TEST_CASE("V2 Mask Median Filter - Empty mask returns empty",
          "[transforms][v2][mask_median_filter][edge]") {
    Mask2D const empty_mask;
    MaskMedianFilterParams const params = tightCanvasParams();
    REQUIRE(applyMedianFilter(empty_mask, params).empty());
}

TEST_CASE("V2 Mask Median Filter - Invalid window size falls back to 3",
          "[transforms][v2][mask_median_filter]") {
    Mask2D const input = horizontalBar(5, 5, 7, 3);

    MaskMedianFilterParams params_even = tightCanvasParams();
    params_even.window_size = 4;
    auto const result_even = applyMedianFilter(input, params_even);

    MaskMedianFilterParams params_default = tightCanvasParams();
    params_default.window_size = 3;
    auto const result_default = applyMedianFilter(input, params_default);

    REQUIRE(result_even.size() == result_default.size());
}

TEST_CASE("V2 Mask Median Filter - Tight canvas matches explicit canvas",
          "[transforms][v2][mask_median_filter][tight_canvas]") {
    Mask2D const input = horizontalBar(10, 20, 15, 5);

    auto const tight = applyMedianFilter(input, tightCanvasParams());
    auto const explicit_canvas =
            applyMedianFilter(input, explicitCanvasParams(100, 100));

    REQUIRE(tight.size() == explicit_canvas.size());
    for (auto const & point: tight) {
        REQUIRE(maskContainsPoint(explicit_canvas, point.x, point.y));
    }
}

TEST_CASE("V2 Mask Median Filter - Rectangular mask via scenario",
          "[transforms][v2][mask_median_filter][scenario]") {
    auto const mask_data = mask_scenarios::rectangular_mask_10x10();
    MaskMedianFilterParams const params = paramsFromMaskData(*mask_data);
    auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));

    auto const result = applyMedianFilter(input_mask, params);

    REQUIRE_FALSE(result.empty());
    REQUIRE(countMaskPoints(result) <= countMaskPoints(input_mask));
}

// ============================================================================
// Tests: Context-aware version
// ============================================================================

TEST_CASE("V2 Mask Median Filter - With Context",
          "[transforms][v2][mask_median_filter][context]") {
    auto const mask_data = mask_scenarios::rectangular_mask_10x10();
    MaskMedianFilterParams const params = paramsFromMaskData(*mask_data);
    auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));

    SECTION("Normal execution reports progress") {
        ComputeContext ctx;
        int progress_val = -1;
        ctx.progress = [&progress_val](int p) { progress_val = p; };
        ctx.is_cancelled = []() { return false; };

        auto const result = applyMedianFilterWithContext(input_mask, params, ctx);

        REQUIRE_FALSE(result.empty());
        REQUIRE(progress_val == 100);
    }

    SECTION("Cancelled execution returns empty mask") {
        ComputeContext ctx;
        ctx.progress = [](int) {};
        ctx.is_cancelled = []() { return true; };

        auto const result = applyMedianFilterWithContext(input_mask, params, ctx);
        REQUIRE(result.empty());
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Mask Median Filter - Registry Integration",
          "[transforms][v2][registry][mask_median_filter]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered") {
        REQUIRE(registry.hasTransform("MedianFilterMask"));
    }

    SECTION("Context-aware transform is registered") {
        REQUIRE(registry.hasTransform("MedianFilterMaskWithContext"));
    }

    SECTION("Can retrieve metadata") {
        auto const * metadata = registry.getMetadata("MedianFilterMask");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "MedianFilterMask");
        REQUIRE(metadata->category == "Image Processing");
    }

    SECTION("Execute via registry") {
        auto const mask_data = mask_scenarios::rectangular_mask_10x10();
        MaskMedianFilterParams const params = paramsFromMaskData(*mask_data);
        auto const input_mask = getFirstMaskAt(*mask_data, TimeFrameIndex(100));

        auto const result = registry.execute<Mask2D, Mask2D, MaskMedianFilterParams>(
                "MedianFilterMask", input_mask, params);

        REQUIRE_FALSE(result.empty());
    }
}
