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
#include "fixtures/scenarios/mask/mask_207252_fixture.hpp"
#include "fixtures/scenarios/mask/skeletonize_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>

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

Mask2D filledSquareMask(uint32_t min_x, uint32_t min_y, uint32_t side_length) {
    std::vector<uint32_t> xs;
    std::vector<uint32_t> ys;
    xs.reserve(static_cast<size_t>(side_length) * static_cast<size_t>(side_length));
    ys.reserve(static_cast<size_t>(side_length) * static_cast<size_t>(side_length));

    for (uint32_t y = min_y; y < min_y + side_length; ++y) {
        for (uint32_t x = min_x; x < min_x + side_length; ++x) {
            xs.push_back(x);
            ys.push_back(y);
        }
    }

    return Mask2D(xs, ys);
}

bool maskContainsPoint(Mask2D const & mask, uint32_t x, uint32_t y) {
    for (auto const & point: mask) {
        if (point.x == x && point.y == y) {
            return true;
        }
    }
    return false;
}

std::pair<uint32_t, uint32_t> minMaskCoordinates(Mask2D const & mask) {
    uint32_t min_x = mask.front().x;
    uint32_t min_y = mask.front().y;
    for (auto const & point: mask) {
        min_x = std::min(min_x, point.x);
        min_y = std::min(min_y, point.y);
    }
    return {min_x, min_y};
}

bool masksHaveSamePoints(Mask2D const & a, Mask2D const & b) {
    if (a.size() != b.size()) {
        return false;
    }

    for (auto const & point: a) {
        if (!maskContainsPoint(b, point.x, point.y)) {
            return false;
        }
    }
    return true;
}

MaskSkeletonizeParams tightCanvasParams() {
    return MaskSkeletonizeParams{};
}

MaskSkeletonizeParams explicitCanvasParams(int width, int height) {
    MaskSkeletonizeParams params;
    params.image_width = width;
    params.image_height = height;
    return params;
}

/**
 * @brief Staggered horizontal bars: each bar's bottom-right meets the next bar's top-left
 *
 * Visual (h=3 bars, w=12 each, vertical step=2):
 * @code
 * x:  0         12        24
 * y0 ###########
 * y1 ###########
 * y2 ###########.........
 * y3 ..........###########
 * y4 ..........###########
 * @endcode
 */
Mask2D staggeredHorizontalRectsMask(
        uint32_t x0,
        uint32_t y0,
        uint32_t bar_width,
        uint32_t bar_height,
        uint32_t bar_count,
        uint32_t x_step,
        uint32_t y_step) {

    std::vector<uint32_t> xs;
    std::vector<uint32_t> ys;

    for (uint32_t bar = 0; bar < bar_count; ++bar) {
        uint32_t const bx = x0 + bar * x_step;
        uint32_t const by = y0 + bar * y_step;
        for (uint32_t y = by; y < by + bar_height; ++y) {
            for (uint32_t x = bx; x < bx + bar_width; ++x) {
                xs.push_back(x);
                ys.push_back(y);
            }
        }
    }

    return Mask2D(xs, ys);
}

struct SkeletonRowBiasStats {
    size_t columns_with_multi_row_mask = 0;
    size_t skeleton_on_top_row = 0;
    size_t skeleton_on_bottom_row = 0;
    size_t skeleton_on_center_row = 0;
    size_t skeleton_on_other_row = 0;
};

SkeletonRowBiasStats analyzeSkeletonRowBias(Mask2D const & mask, Mask2D const & skeleton) {
    std::map<uint32_t, std::set<uint32_t>> mask_y_by_x;
    std::map<uint32_t, std::set<uint32_t>> skeleton_y_by_x;

    for (auto const & point: mask) {
        mask_y_by_x[point.x].insert(point.y);
    }
    for (auto const & point: skeleton) {
        skeleton_y_by_x[point.x].insert(point.y);
    }

    SkeletonRowBiasStats stats;
    for (auto const & [x, mask_ys_set]: mask_y_by_x) {
        if (mask_ys_set.size() < 2) {
            continue;
        }

        ++stats.columns_with_multi_row_mask;

        std::vector<uint32_t> const mask_ys(mask_ys_set.begin(), mask_ys_set.end());
        uint32_t const top_y = mask_ys.front();
        uint32_t const bottom_y = mask_ys.back();
        uint32_t const center_y = (top_y + bottom_y) / 2;

        auto const skel_it = skeleton_y_by_x.find(x);
        if (skel_it == skeleton_y_by_x.end()) {
            continue;
        }

        for (uint32_t const skel_y: skel_it->second) {
            if (skel_y == top_y) {
                ++stats.skeleton_on_top_row;
            } else if (skel_y == bottom_y) {
                ++stats.skeleton_on_bottom_row;
            } else if (skel_y == center_y) {
                ++stats.skeleton_on_center_row;
            } else {
                ++stats.skeleton_on_other_row;
            }
        }
    }

    return stats;
}

float meanSkeletonRowInColumnRange(Mask2D const & skeleton, uint32_t x_min, uint32_t x_max) {
    float sum = 0.0f;
    size_t count = 0;
    for (auto const & point: skeleton) {
        if (point.x >= x_min && point.x < x_max) {
            sum += static_cast<float>(point.y);
            ++count;
        }
    }
    return count > 0 ? sum / static_cast<float>(count) : -1.0f;
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
// Tests: Tight canvas coordinate preservation
// ============================================================================

TEST_CASE("V2 Mask Skeletonize - Tight canvas preserves absolute coordinates",
          "[transforms][v2][mask_skeletonize][tight_canvas][coordinates]") {

    SECTION("3x3 square at (3,3) skeletonizes to center pixel (4,4)") {
        Mask2D const input = filledSquareMask(3, 3, 3);
        MaskSkeletonizeParams const params = tightCanvasParams();

        auto const result = skeletonizeMask(input, params);

        REQUIRE(result.size() == 1);
        REQUIRE(maskContainsPoint(result, 4, 4));
        auto const [min_x, min_y] = minMaskCoordinates(result);
        REQUIRE(min_x == 4);
        REQUIRE(min_y == 4);
    }

    SECTION("Tight canvas matches explicit canvas for offset square") {
        Mask2D const input = filledSquareMask(3, 3, 3);
        MaskSkeletonizeParams const tight_params = tightCanvasParams();
        MaskSkeletonizeParams const explicit_params = explicitCanvasParams(100, 100);

        auto const tight_result = skeletonizeMask(input, tight_params);
        auto const explicit_result = skeletonizeMask(input, explicit_params);

        REQUIRE_FALSE(tight_result.empty());
        REQUIRE_FALSE(explicit_result.empty());
        REQUIRE(masksHaveSamePoints(tight_result, explicit_result));
    }

    SECTION("Tight canvas matches explicit canvas for mask at origin") {
        Mask2D const input = filledSquareMask(0, 0, 5);
        MaskSkeletonizeParams const tight_params = tightCanvasParams();
        MaskSkeletonizeParams const explicit_params = explicitCanvasParams(64, 64);

        auto const tight_result = skeletonizeMask(input, tight_params);
        auto const explicit_result = skeletonizeMask(input, explicit_params);

        REQUIRE_FALSE(tight_result.empty());
        REQUIRE_FALSE(explicit_result.empty());
        REQUIRE(masksHaveSamePoints(tight_result, explicit_result));
    }
}

TEST_CASE("V2 Mask Skeletonize - mask_207252.png staggered whisker pattern",
          "[transforms][v2][mask_skeletonize][mask_207252]") {

    Mask2D const input = mask_207252_fixture::maskFromImage();
    REQUIRE(input.size() == 1077);

    auto const [min_point, max_point] = get_bounding_box(input);
    INFO("input bbox x=[" << min_point.x << "," << max_point.x << "] y=["
                          << min_point.y << "," << max_point.y << "]");

    SECTION("Tight canvas skeleton row bias") {
        MaskSkeletonizeParams const params = tightCanvasParams();
        auto const skeleton = skeletonizeMask(input, params);

        REQUIRE_FALSE(skeleton.empty());
        INFO("skeleton_points=" << skeleton.size());

        auto const stats = analyzeSkeletonRowBias(input, skeleton);
        INFO("columns_with_multi_row_mask=" << stats.columns_with_multi_row_mask);
        INFO("skeleton_on_top_row=" << stats.skeleton_on_top_row);
        INFO("skeleton_on_bottom_row=" << stats.skeleton_on_bottom_row);
        INFO("skeleton_on_center_row=" << stats.skeleton_on_center_row);
        INFO("skeleton_on_other_row=" << stats.skeleton_on_other_row);

        REQUIRE(stats.columns_with_multi_row_mask > 0);
        // Document observed behavior: Zhang–Suen skeleton hugs the top row of 2–3 px bars
        REQUIRE(stats.skeleton_on_top_row > stats.skeleton_on_center_row);
        REQUIRE(stats.skeleton_on_top_row > stats.skeleton_on_bottom_row);
    }

    SECTION("Tight canvas matches explicit 640x480 canvas") {
        auto const tight = skeletonizeMask(input, tightCanvasParams());
        auto const explicit_canvas =
                skeletonizeMask(input, explicitCanvasParams(640, 480));

        REQUIRE(masksHaveSamePoints(tight, explicit_canvas));
    }

    SECTION("Sample column y-values are integer pixel rows") {
        Mask2D const skeleton = skeletonizeMask(input, tightCanvasParams());
        for (auto const & point: skeleton) {
            REQUIRE(point.x >= min_point.x);
            REQUIRE(point.y >= min_point.y);
        }

        // Example interior column from the real mask image
        std::set<uint32_t> mask_ys;
        std::set<uint32_t> skel_ys;
        uint32_t const sample_x = 327;
        for (auto const & point: input) {
            if (point.x == sample_x) {
                mask_ys.insert(point.y);
            }
        }
        for (auto const & point: skeleton) {
            if (point.x == sample_x) {
                skel_ys.insert(point.y);
            }
        }

        REQUIRE(mask_ys.size() >= 2);
        REQUIRE_FALSE(skel_ys.empty());
        INFO("x=" << sample_x << " mask_ys={"
                  << *mask_ys.begin() << ".."
                  << *std::prev(mask_ys.end()) << "} skel_ys count=" << skel_ys.size());
        for (uint32_t const y: skel_ys) {
            REQUIRE(mask_ys.count(y) == 1);
        }
    }
}

TEST_CASE("V2 Mask Skeletonize - Staggered horizontal rectangles skeleton topology",
          "[transforms][v2][mask_skeletonize][staggered][diagnostic]") {

    auto analyzeBars = [](Mask2D const & input, char const * label,
                          uint32_t x0, uint32_t y0, uint32_t bar_width, uint32_t bar_height,
                          uint32_t bar_count, uint32_t x_step, uint32_t y_step) {
        MaskSkeletonizeParams const params = tightCanvasParams();
        auto const skeleton = skeletonizeMask(input, params);
        INFO(label << " skeleton_points=" << skeleton.size());

        for (uint32_t bar = 0; bar < bar_count; ++bar) {
            uint32_t const bx = x0 + bar * x_step;
            uint32_t const by = y0 + bar * y_step;
            uint32_t const geometric_center_y = by + bar_height / 2;
            uint32_t const sample_x_min = bx + 2;
            uint32_t const sample_x_max = bx + bar_width - 2;
            float const mean_skeleton_y =
                    meanSkeletonRowInColumnRange(skeleton, sample_x_min, sample_x_max);

            INFO(label << " bar=" << bar << " center_y=" << geometric_center_y
                       << " mean_skel_y=" << mean_skeleton_y
                       << " delta=" << (mean_skeleton_y - static_cast<float>(geometric_center_y)));
        }

        return skeleton;
    };

    SECTION("Overlapping junction rows") {
        uint32_t const bar_width = 20;
        uint32_t const bar_height = 5;
        Mask2D input = staggeredHorizontalRectsMask(5, 5, bar_width, bar_height, 3, 12, 4);
        auto const skeleton = analyzeBars(input, "overlap", 5, 5, bar_width, bar_height, 3, 12, 4);
        REQUIRE_FALSE(skeleton.empty());
    }

    SECTION("Corner-touch junction (no horizontal overlap between bars)") {
        uint32_t const bar_width = 20;
        uint32_t const bar_height = 5;
        Mask2D input;
        auto addBar = [](Mask2D & mask, uint32_t bx, uint32_t by, uint32_t w, uint32_t h) {
            for (uint32_t y = by; y < by + h; ++y) {
                for (uint32_t x = bx; x < bx + w; ++x) {
                    mask.push_back({x, y});
                }
            }
        };
        addBar(input, 5, 5, bar_width, bar_height);   // br at (24, 9)
        addBar(input, 24, 9, bar_width, bar_height);   // tl at (24, 9)
        addBar(input, 43, 13, bar_width, bar_height);

        auto const skeleton = analyzeBars(input, "corner", 5, 5, bar_width, bar_height, 3, 19, 4);
        REQUIRE_FALSE(skeleton.empty());
    }

    SECTION("Thin bars (height=3) descending stair") {
        Mask2D input;
        auto addBar = [](Mask2D & mask, uint32_t bx, uint32_t by, uint32_t w, uint32_t h) {
            for (uint32_t y = by; y < by + h; ++y) {
                for (uint32_t x = bx; x < bx + w; ++x) {
                    mask.push_back({x, y});
                }
            }
        };
        addBar(input, 5, 5, 30, 3);
        addBar(input, 30, 7, 30, 3);
        addBar(input, 55, 9, 30, 3);

        auto const skeleton = analyzeBars(input, "thin", 5, 5, 30, 3, 3, 25, 2);
        REQUIRE_FALSE(skeleton.empty());
    }
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
