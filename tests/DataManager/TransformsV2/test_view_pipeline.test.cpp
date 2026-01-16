#include "transforms/v2/algorithms/MaskArea/MaskArea.hpp"
#include "transforms/v2/algorithms/SumReduction/SumReduction.hpp"
#include "transforms/v2/detail/ContainerTransform.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
//#include "transforms/v2/core/ElementTransform.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"


#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "CoreGeometry/masks.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <memory>
#include <ranges>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * @brief Create test mask data with known areas
 */
inline std::shared_ptr<MaskData> createTestMaskData() {
    std::vector<int> times = {0, 10, 20, 30};
    auto time_frame = std::make_shared<TimeFrame>(times);

    auto mask_data = std::make_shared<MaskData>();
    mask_data->setTimeFrame(time_frame);

    // Time 0: one mask with 4 pixels (area = 4)
    {
        Mask2D mask({Point2D<uint32_t>{0, 0},
                     Point2D<uint32_t>{0, 1},
                     Point2D<uint32_t>{1, 0},
                     Point2D<uint32_t>{1, 1}});
        mask_data->addAtTime(TimeFrameIndex(0), mask, NotifyObservers::No);
    }

    // Time 10: two masks (areas = 2, 3)
    {
        Mask2D mask1({Point2D<uint32_t>{0, 0},
                      Point2D<uint32_t>{1, 0}});
        mask_data->addAtTime(TimeFrameIndex(10), mask1, NotifyObservers::No);

        Mask2D mask2({Point2D<uint32_t>{0, 0},
                      Point2D<uint32_t>{0, 1},
                      Point2D<uint32_t>{0, 2}});
        mask_data->addAtTime(TimeFrameIndex(10), mask2, NotifyObservers::No);
    }

    // Time 20: one mask with 6 pixels (area = 6)
    {
        Mask2D mask({Point2D<uint32_t>{0, 0}, Point2D<uint32_t>{0, 1},
                     Point2D<uint32_t>{1, 0}, Point2D<uint32_t>{1, 1},
                     Point2D<uint32_t>{2, 0}, Point2D<uint32_t>{2, 1}});
        mask_data->addAtTime(TimeFrameIndex(20), mask, NotifyObservers::No);
    }

    return std::move(mask_data);
}

// ============================================================================
// Tests: RaggedAnalogTimeSeries::elements() View
// ============================================================================

TEST_CASE("TransformsV2 - RaggedAnalogTimeSeries elements() view", "[transforms][v2][view]") {
    std::vector<int> times = {0, 10, 20};
    auto time_frame = std::make_shared<TimeFrame>(times);

    RaggedAnalogTimeSeries ragged;
    ragged.setTimeFrame(time_frame);

    // Add data: time 0 has [1.0, 2.0], time 10 has [3.0], time 20 has [4.0, 5.0, 6.0]
    ragged.setDataAtTime(TimeFrameIndex(0), std::vector<float>{1.0f, 2.0f}, NotifyObservers::No);
    ragged.setDataAtTime(TimeFrameIndex(10), std::vector<float>{3.0f}, NotifyObservers::No);
    ragged.setDataAtTime(TimeFrameIndex(20), std::vector<float>{4.0f, 5.0f, 6.0f}, NotifyObservers::No);

    SECTION("elements() flattens ragged structure") {
        std::vector<std::pair<TimeFrameIndex, float>> collected;

        for (auto [time, value]: ragged.elements()) {
            collected.emplace_back(time, value);
        }

        REQUIRE(collected.size() == 6);// Total of 6 float values

        // Check time 0 entries
        REQUIRE(collected[0].first == TimeFrameIndex(0));
        REQUIRE(collected[0].second == 1.0f);
        REQUIRE(collected[1].first == TimeFrameIndex(0));
        REQUIRE(collected[1].second == 2.0f);

        // Check time 10 entry
        REQUIRE(collected[2].first == TimeFrameIndex(10));
        REQUIRE(collected[2].second == 3.0f);

        // Check time 20 entries
        REQUIRE(collected[3].first == TimeFrameIndex(20));
        REQUIRE(collected[3].second == 4.0f);
        REQUIRE(collected[4].first == TimeFrameIndex(20));
        REQUIRE(collected[4].second == 5.0f);
        REQUIRE(collected[5].first == TimeFrameIndex(20));
        REQUIRE(collected[5].second == 6.0f);
    }

    SECTION("elements() is lazy (composes with views)") {
        // Transform the view without materializing
        auto doubled_view = ragged.elements() | std::views::transform([](auto pair) {
                                return std::make_pair(pair.first, pair.second * 2.0f);
                            });

        // Collect results
        std::vector<float> doubled_values;
        for (auto [time, value]: doubled_view) {
            doubled_values.push_back(value);
        }

        REQUIRE(doubled_values.size() == 6);
        REQUIRE(doubled_values[0] == 2.0f); // 1.0 * 2
        REQUIRE(doubled_values[1] == 4.0f); // 2.0 * 2
        REQUIRE(doubled_values[2] == 6.0f); // 3.0 * 2
        REQUIRE(doubled_values[3] == 8.0f); // 4.0 * 2
        REQUIRE(doubled_values[4] == 10.0f);// 5.0 * 2
        REQUIRE(doubled_values[5] == 12.0f);// 6.0 * 2
    }
}

TEST_CASE("TransformsV2 - RaggedAnalogTimeSeries time_slices() view", "[transforms][v2][view]") {
    RaggedAnalogTimeSeries ragged;
    ragged.setDataAtTime(TimeFrameIndex(0), std::vector<float>{1.0f, 2.0f}, NotifyObservers::No);
    ragged.setDataAtTime(TimeFrameIndex(10), std::vector<float>{3.0f}, NotifyObservers::No);

    SECTION("time_slices() returns span per time") {
        std::vector<std::pair<TimeFrameIndex, std::vector<float>>> collected;

        for (auto [time, span]: ragged.time_slices()) {
            collected.emplace_back(time, std::vector<float>(span.begin(), span.end()));
        }

        REQUIRE(collected.size() == 2);

        REQUIRE(collected[0].first == TimeFrameIndex(0));
        REQUIRE(collected[0].second.size() == 2);
        REQUIRE(collected[0].second[0] == 1.0f);
        REQUIRE(collected[0].second[1] == 2.0f);

        REQUIRE(collected[1].first == TimeFrameIndex(10));
        REQUIRE(collected[1].second.size() == 1);
        REQUIRE(collected[1].second[0] == 3.0f);
    }
}

// ============================================================================
// Tests: RaggedTimeSeries<T> Range Constructor
// ============================================================================

TEST_CASE("TransformsV2 - RaggedTimeSeries range constructor", "[transforms][v2][view]") {
    std::vector<int> times = {0, 10, 20};
    auto time_frame = std::make_shared<TimeFrame>(times);

    SECTION("Construct MaskData from (TimeFrameIndex, Mask2D) pairs") {

        // Construct MaskData from range using shared_ptr
        auto mask_data = std::make_shared<MaskData>();

        mask_data->addAtTime(TimeFrameIndex(0),
                             Mask2D({Point2D<uint32_t>{0, 0}, Point2D<uint32_t>{1, 1}}),
                             NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(10),
                             Mask2D({Point2D<uint32_t>{2, 2}}),
                             NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(20),
                             Mask2D({Point2D<uint32_t>{3, 3}, Point2D<uint32_t>{4, 4}, Point2D<uint32_t>{5, 5}}),
                             NotifyObservers::No);
        mask_data->setTimeFrame(time_frame);

        // Verify data was added correctly
        REQUIRE(mask_data->getTimeCount() == 3);

        auto data_at_0 = mask_data->getAtTime(TimeFrameIndex(0));
        std::vector<Mask2D> masks_at_0(data_at_0.begin(), data_at_0.end());
        REQUIRE(masks_at_0.size() == 1);
        REQUIRE(masks_at_0[0].size() == 2);

        auto data_at_10 = mask_data->getAtTime(TimeFrameIndex(10));
        std::vector<Mask2D> masks_at_10(data_at_10.begin(), data_at_10.end());
        REQUIRE(masks_at_10.size() == 1);
        REQUIRE(masks_at_10[0].size() == 1);

        auto data_at_20 = mask_data->getAtTime(TimeFrameIndex(20));
        std::vector<Mask2D> masks_at_20(data_at_20.begin(), data_at_20.end());
        REQUIRE(masks_at_20.size() == 1);
        REQUIRE(masks_at_20[0].size() == 3);
    }

    SECTION("Construct from transformed view") {
        auto source_data = createTestMaskData();

        // Create a view that transforms masks (e.g., scale by 2)
        auto transformed_view = source_data->elements() | std::views::transform([](auto const & elem) {
                                    TimeFrameIndex time = elem.first;
                                    Mask2D const & mask = elem.second.data;

                                    // Create a transformed mask (scale coordinates by 2)
                                    std::vector<Point2D<uint32_t>> scaled_points;
                                    for (auto const & pt: mask.points()) {
                                        scaled_points.emplace_back(pt.x * 2, pt.y * 2);
                                    }

                                    return std::make_pair(time, Mask2D(scaled_points));
                                });

        // Construct new MaskData from transformed view
        auto transformed_data = std::make_shared<MaskData>(transformed_view);
        transformed_data->setTimeFrame(source_data->getTimeFrame());

        // Verify transformation was applied
        REQUIRE(transformed_data->getTimeCount() == source_data->getTimeCount());

        // Check first mask was scaled
        auto original_first = source_data->getAtTime(TimeFrameIndex(0));
        auto transformed_first = transformed_data->getAtTime(TimeFrameIndex(0));

        std::vector<Mask2D> orig_masks(original_first.begin(), original_first.end());
        std::vector<Mask2D> trans_masks(transformed_first.begin(), transformed_first.end());

        REQUIRE(orig_masks.size() == 1);
        REQUIRE(trans_masks.size() == 1);
        REQUIRE(orig_masks[0].size() == trans_masks[0].size());

        // Coordinates should be scaled
        auto const & orig_points = orig_masks[0].points();
        auto const & trans_points = trans_masks[0].points();
        REQUIRE(trans_points[0].x == orig_points[0].x * 2);
        REQUIRE(trans_points[0].y == orig_points[0].y * 2);
    }
}

// ============================================================================
// Tests: View-Based Single Transform
// ============================================================================

TEST_CASE("TransformsV2 - applyElementTransformView returns lazy view", "[transforms][v2][view]") {
    auto mask_data = createTestMaskData();
    MaskAreaParams params;

    SECTION("View is lazy - no computation until accessed") {
        // This just creates a view - no transforms execute yet
        auto view = applyElementTransformView<MaskData, Mask2D, float, MaskAreaParams>(
                *mask_data, "CalculateMaskArea", params);

        // Now iterate and collect results (transforms execute here)
        std::vector<std::pair<TimeFrameIndex, float>> results;
        for (auto [time, area]: view) {
            results.emplace_back(time, area);
        }

        // Should have 4 masks total (1 + 2 + 1 + 0)
        REQUIRE(results.size() == 4);

        // Check areas match expected
        REQUIRE(results[0].first == TimeFrameIndex(0));
        REQUIRE(results[0].second == 4.0f);

        REQUIRE(results[1].first == TimeFrameIndex(10));
        REQUIRE(results[1].second == 2.0f);
        REQUIRE(results[2].first == TimeFrameIndex(10));
        REQUIRE(results[2].second == 3.0f);

        REQUIRE(results[3].first == TimeFrameIndex(20));
        REQUIRE(results[3].second == 6.0f);
    }

    SECTION("View can be chained with other views") {
        auto area_view = applyElementTransformView<MaskData, Mask2D, float, MaskAreaParams>(
                *mask_data, "CalculateMaskArea", params);

        // Chain another transformation without materializing
        auto doubled_view = area_view | std::views::transform([](auto pair) {
                                return std::make_pair(pair.first, pair.second * 2.0f);
                            });

        // Materialize into container
        auto result = std::make_shared<RaggedAnalogTimeSeries>(doubled_view);
        result->setTimeFrame(mask_data->getTimeFrame());

        // Verify doubled areas
        auto data_at_0 = result->getDataAtTime(TimeFrameIndex(0));
        REQUIRE(data_at_0.size() == 1);
        REQUIRE(data_at_0[0] == 8.0f);// 4 * 2

        auto data_at_10 = result->getDataAtTime(TimeFrameIndex(10));
        REQUIRE(data_at_10.size() == 2);
        REQUIRE(data_at_10[0] == 4.0f);// 2 * 2
        REQUIRE(data_at_10[1] == 6.0f);// 3 * 2
    }

    SECTION("View can be filtered before materialization") {
        auto area_view = applyElementTransformView<MaskData, Mask2D, float, MaskAreaParams>(
                *mask_data, "CalculateMaskArea", params);

        // Filter out small areas (< 4)
        auto filtered_view = area_view | std::views::filter([](auto pair) {
                                 return pair.second >= 4.0f;
                             });

        std::vector<float> filtered_areas;
        for (auto [time, area]: filtered_view) {
            filtered_areas.push_back(area);
        }

        // Should only have areas >= 4: [4.0, 6.0]
        REQUIRE(filtered_areas.size() == 2);
        REQUIRE(filtered_areas[0] == 4.0f);
        REQUIRE(filtered_areas[1] == 6.0f);
    }
}

// ============================================================================
// Tests: Pipeline executeAsView()
// ============================================================================

TEST_CASE("TransformsV2 - Pipeline executeAsView returns lazy view", "[transforms][v2][view][pipeline]") {
    auto mask_data = createTestMaskData();

    SECTION("Single-step pipeline as view") {
        TransformPipeline pipeline;
        pipeline.addStep("CalculateMaskArea", MaskAreaParams{});

        // Get lazy view
        auto view = pipeline.executeAsViewTyped<MaskData, float>(*mask_data);

        // Collect results
        std::vector<float> areas;
        for (auto [time, area]: view) {
            areas.push_back(area);
        }

        REQUIRE(areas.size() == 4);
        REQUIRE(areas[0] == 4.0f);
        REQUIRE(areas[1] == 2.0f);
        REQUIRE(areas[2] == 3.0f);
        REQUIRE(areas[3] == 6.0f);
    }

    SECTION("View can be materialized into container") {
        TransformPipeline pipeline;
        pipeline.addStep("CalculateMaskArea", MaskAreaParams{});

        auto view = pipeline.executeAsViewTyped<MaskData, float>(*mask_data);

        // Materialize into RaggedAnalogTimeSeries
        auto result = std::make_shared<RaggedAnalogTimeSeries>(view);
        result->setTimeFrame(mask_data->getTimeFrame());

        // Verify materialized data
        REQUIRE(result->getNumTimePoints() == 3);

        auto data_at_0 = result->getDataAtTime(TimeFrameIndex(0));
        REQUIRE(data_at_0.size() == 1);
        REQUIRE(data_at_0[0] == 4.0f);

        auto data_at_10 = result->getDataAtTime(TimeFrameIndex(10));
        REQUIRE(data_at_10.size() == 2);
        REQUIRE(data_at_10[0] == 2.0f);
        REQUIRE(data_at_10[1] == 3.0f);
    }

    SECTION("View can be chained with additional transformations") {
        TransformPipeline pipeline;
        pipeline.addStep("CalculateMaskArea", MaskAreaParams{});

        auto view = pipeline.executeAsViewTyped<MaskData, float>(*mask_data);

        // Add post-processing via view transformation
        auto scaled_view = view | std::views::transform([](auto pair) {
                               return std::make_pair(pair.first, pair.second * 0.5f);
                           });

        std::vector<float> scaled_areas;
        for (auto [time, area]: scaled_view) {
            scaled_areas.push_back(area);
        }

        REQUIRE(scaled_areas.size() == 4);
        REQUIRE(scaled_areas[0] == 2.0f);// 4 * 0.5
        REQUIRE(scaled_areas[1] == 1.0f);// 2 * 0.5
        REQUIRE(scaled_areas[2] == 1.5f);// 3 * 0.5
        REQUIRE(scaled_areas[3] == 3.0f);// 6 * 0.5
    }
}

TEST_CASE("TransformsV2 - Pipeline executeAsView rejects time-grouped transforms", "[transforms][v2][view][pipeline]") {
    RaggedAnalogTimeSeries ragged;
    ragged.setDataAtTime(TimeFrameIndex(0), std::vector<float>{1.0f, 2.0f}, NotifyObservers::No);

    TransformPipeline pipeline;
    pipeline.addStep("SumReduction", SumReductionParams{});// Time-grouped transform

    // Should throw because SumReduction is time-grouped
    REQUIRE_THROWS_AS(
            pipeline.executeAsView(ragged),
            std::runtime_error);
}

// ============================================================================
// Tests: Performance - View vs Materialized
// ============================================================================

TEST_CASE("TransformsV2 - View-based pipeline avoids intermediate materializations", "[transforms][v2][view][performance]") {
    auto mask_data = createTestMaskData();

    // Note: Traditional execute() is tested elsewhere - this test focuses on view benefits

    SECTION("View-based path materializes only at consumer") {
        TransformPipeline pipeline;
        pipeline.addStep("CalculateMaskArea", MaskAreaParams{});

        // This creates a lazy view - no container yet
        auto view = pipeline.executeAsViewTyped<MaskData, float>(*mask_data);

        // Process elements one at a time (streaming)
        size_t count = 0;
        float sum = 0.0f;

        for (auto [time, area]: view) {
            // Transform executes here, one element at a time
            count++;
            sum += area;
        }

        REQUIRE(count == 4);
        REQUIRE_THAT(sum, Catch::Matchers::WithinRel(15.0f, 0.001f));// 4 + 2 + 3 + 6 = 15

        // No container was ever created!
    }
}

// ============================================================================
// Tests: Integration - Full View Pipeline
// ============================================================================

TEST_CASE("TransformsV2 - Complete view-based workflow", "[transforms][v2][view][integration]") {
    auto mask_data = createTestMaskData();

    SECTION("Entry as container, propagate as view, exit as container") {
        // Step 1: Entry - start with container
        REQUIRE(mask_data->getTimeCount() == 3);

        // Step 2: Transform to view
        auto view = applyElementTransformView<MaskData, Mask2D, float, MaskAreaParams>(
                *mask_data, "CalculateMaskArea", MaskAreaParams{});

        // Step 3: Propagate view through transformations
        auto filtered_view = view | std::views::filter([](auto pair) { return pair.second > 2.5f; }) | std::views::transform([](auto pair) {
                                 return std::make_pair(pair.first, pair.second * 10.0f);// Scale by 10
                             });

        // Step 4: Exit - materialize to container
        auto result = std::make_shared<RaggedAnalogTimeSeries>(filtered_view);
        result->setTimeFrame(mask_data->getTimeFrame());

        // Verify final result
        // Note: getNumTimePoints() returns 3 because time 10 still has one element (area=3) that passed filter
        REQUIRE(result->getNumTimePoints() == 3);

        auto data_at_0 = result->getDataAtTime(TimeFrameIndex(0));
        REQUIRE(data_at_0.size() == 1);
        REQUIRE(data_at_0[0] == 40.0f);// 4.0 * 10

        auto data_at_10 = result->getDataAtTime(TimeFrameIndex(10));
        REQUIRE(data_at_10.size() == 1);
        REQUIRE(data_at_10[0] == 30.0f);// 3.0 * 10 (only the 3.0 area passes filter)

        auto data_at_20 = result->getDataAtTime(TimeFrameIndex(20));
        REQUIRE(data_at_20.size() == 1);
        REQUIRE(data_at_20[0] == 60.0f);// 6.0 * 10
    }
}
