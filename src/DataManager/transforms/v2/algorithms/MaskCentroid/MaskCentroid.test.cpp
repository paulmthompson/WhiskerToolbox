#include "MaskCentroid.hpp"

#include "DataManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"

#include "fixtures/scenarios/mask/centroid_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <iostream>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Tests: MaskCentroidParams JSON Loading
// ============================================================================

TEST_CASE("MaskCentroidParams - Load empty JSON (uses all defaults)", "[transforms][v2][params][json][centroid]") {
    std::string json = "{}";
    
    auto result = loadParametersFromJson<MaskCentroidParams>(json);
    
    REQUIRE(result);
    // Currently no parameters, so just verify parsing works
}

TEST_CASE("MaskCentroidParams - JSON round-trip preserves values", "[transforms][v2][params][json][centroid]") {
    MaskCentroidParams original;
    
    // Serialize
    std::string json = saveParametersToJson(original);
    
    // Deserialize
    auto result = loadParametersFromJson<MaskCentroidParams>(json);
    REQUIRE(result);
    // Currently no parameters to verify, just check parsing works
}

TEST_CASE("MaskCentroidParams - Reject malformed JSON", "[transforms][v2][params][json][validation][centroid]") {
    std::string json = R"({
        "invalid
    })";
    
    auto result = loadParametersFromJson<MaskCentroidParams>(json);
    
    REQUIRE(!result);
}

// ============================================================================
// Core Functionality Tests (using scenarios shared with V1)
// ============================================================================

TEST_CASE("TransformsV2 - Centroid: Empty mask data via scenario", 
          "[transforms][v2][centroid][scenario]") {
    auto mask_data = mask_scenarios::empty_mask_data();
    auto& registry = ElementRegistry::instance();
    
    MaskCentroidParams params;
    auto transform_fn = registry.getTransformFunction<Mask2D, Point2D<float>, MaskCentroidParams>(
        "CalculateMaskCentroid", params);
    
    // Empty MaskData should produce empty result
    auto result = std::make_shared<PointData>();
    for (auto const& [time, entry] : mask_data->elements()) {
        Point2D<float> centroid = transform_fn(entry.data);
        result->addAtTime(time, centroid, NotifyObservers::No);
    }
    
    REQUIRE(result->getTimesWithData().empty());
}

TEST_CASE("TransformsV2 - Centroid: Single mask at one timestamp (triangle) via scenario", 
          "[transforms][v2][centroid][scenario]") {
    auto mask_data = mask_scenarios::single_mask_triangle();
    auto& registry = ElementRegistry::instance();
    
    MaskCentroidParams params;
    auto transform_fn = registry.getTransformFunction<Mask2D, Point2D<float>, MaskCentroidParams>(
        "CalculateMaskCentroid", params);
    
    auto result = std::make_shared<PointData>();
    for (auto const& [time, entry] : mask_data->elements()) {
        Point2D<float> centroid = transform_fn(entry.data);
        result->addAtTime(time, centroid, NotifyObservers::No);
    }
    result->setTimeFrame(mask_data->getTimeFrame());
    
    auto const& times = result->getTimesWithData();
    REQUIRE(times.size() == 1);
    REQUIRE(*times.begin() == TimeFrameIndex(10));
    
    auto const& points = result->getAtTime(TimeFrameIndex(10));
    REQUIRE(points.size() == 1);
    
    // Centroid of triangle with vertices (0,0), (3,0), (0,3) should be (1,1)
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

TEST_CASE("TransformsV2 - Centroid: Multiple masks at one timestamp via scenario", 
          "[transforms][v2][centroid][scenario]") {
    auto mask_data = mask_scenarios::multiple_masks_single_timestamp_centroid();
    auto& registry = ElementRegistry::instance();
    
    MaskCentroidParams params;
    auto transform_fn = registry.getTransformFunction<Mask2D, Point2D<float>, MaskCentroidParams>(
        "CalculateMaskCentroid", params);
    
    auto result = std::make_shared<PointData>();
    for (auto const& [time, entry] : mask_data->elements()) {
        Point2D<float> centroid = transform_fn(entry.data);
        result->addAtTime(time, centroid, NotifyObservers::No);
    }
    result->setTimeFrame(mask_data->getTimeFrame());
    
    auto const& times = result->getTimesWithData();
    REQUIRE(times.size() == 1);
    REQUIRE(*times.begin() == TimeFrameIndex(20));
    
    auto const& points_view = result->getAtTime(TimeFrameIndex(20));
    std::vector<Point2D<float>> points(points_view.begin(), points_view.end());
    REQUIRE(points.size() == 2);  // Two masks = two centroids
    
    // Sort points by x coordinate for consistent testing
    auto sorted_points = points;
    std::sort(sorted_points.begin(), sorted_points.end(),
              [](auto const & a, auto const & b) { return a.x < b.x; });
    
    // First centroid: (0+1+0+1)/4 = 0.5, (0+0+1+1)/4 = 0.5
    REQUIRE_THAT(sorted_points[0].x, Catch::Matchers::WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(sorted_points[0].y, Catch::Matchers::WithinAbs(0.5f, 0.001f));
    
    // Second centroid: (4+5+4+5)/4 = 4.5, (4+4+5+5)/4 = 4.5
    REQUIRE_THAT(sorted_points[1].x, Catch::Matchers::WithinAbs(4.5f, 0.001f));
    REQUIRE_THAT(sorted_points[1].y, Catch::Matchers::WithinAbs(4.5f, 0.001f));
}

TEST_CASE("TransformsV2 - Centroid: Masks across multiple timestamps via scenario", 
          "[transforms][v2][centroid][scenario]") {
    auto mask_data = mask_scenarios::masks_multiple_timestamps_centroid();
    auto& registry = ElementRegistry::instance();
    
    MaskCentroidParams params;
    auto transform_fn = registry.getTransformFunction<Mask2D, Point2D<float>, MaskCentroidParams>(
        "CalculateMaskCentroid", params);
    
    auto result = std::make_shared<PointData>();
    for (auto const& [time, entry] : mask_data->elements()) {
        Point2D<float> centroid = transform_fn(entry.data);
        result->addAtTime(time, centroid, NotifyObservers::No);
    }
    result->setTimeFrame(mask_data->getTimeFrame());
    
    auto const& times = result->getTimesWithData();
    REQUIRE(times.size() == 2);
    
    // Check timestamp 30 centroid: (0+2+4)/3 = 2, (0+0+0)/3 = 0
    auto const& points30 = result->getAtTime(TimeFrameIndex(30));
    REQUIRE(points30.size() == 1);
    REQUIRE_THAT(points30[0].x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(points30[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    
    // Check timestamp 40 centroid: (1+1+1)/3 = 1, (0+3+6)/3 = 3
    auto const& points40 = result->getAtTime(TimeFrameIndex(40));
    REQUIRE(points40.size() == 1);
    REQUIRE_THAT(points40[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(points40[0].y, Catch::Matchers::WithinAbs(3.0f, 0.001f));
}

TEST_CASE("TransformsV2 - Centroid: Verify image size is preserved via scenario",
          "[transforms][v2][centroid][scenario]") {
    auto mask_data = mask_scenarios::mask_with_image_size_centroid();
    auto& registry = ElementRegistry::instance();
    
    MaskCentroidParams params;
    auto transform_fn = registry.getTransformFunction<Mask2D, Point2D<float>, MaskCentroidParams>(
        "CalculateMaskCentroid", params);
    
    auto result = std::make_shared<PointData>();
    result->setImageSize(mask_data->getImageSize());
    
    for (auto const& [time, entry] : mask_data->elements()) {
        Point2D<float> centroid = transform_fn(entry.data);
        result->addAtTime(time, centroid, NotifyObservers::No);
    }
    result->setTimeFrame(mask_data->getTimeFrame());
    
    // Verify image size is copied
    REQUIRE(result->getImageSize().width == 640);
    REQUIRE(result->getImageSize().height == 480);
    
    // Verify calculation is correct: (100+200+300)/3 = 200, (100+150+200)/3 = 150
    auto const& points = result->getAtTime(TimeFrameIndex(100));
    REQUIRE(points.size() == 1);
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(150.0f, 0.001f));
}

// ============================================================================
// Edge Cases (using scenarios)
// ============================================================================

TEST_CASE("TransformsV2 - Centroid: Masks with zero points via scenario",
          "[transforms][v2][centroid][edge][scenario]") {
    auto mask_data = mask_scenarios::empty_mask_at_timestamp_centroid();
    auto& registry = ElementRegistry::instance();
    
    MaskCentroidParams params;
    auto transform_fn = registry.getTransformFunction<Mask2D, Point2D<float>, MaskCentroidParams>(
        "CalculateMaskCentroid", params);
    
    auto result = std::make_shared<PointData>();
    for (auto const& [time, entry] : mask_data->elements()) {
        Point2D<float> centroid = transform_fn(entry.data);
        // V2 adds a (0,0) centroid for empty masks
        result->addAtTime(time, centroid, NotifyObservers::No);
    }
    result->setTimeFrame(mask_data->getTimeFrame());
    
    // V2 includes empty masks with (0,0) centroid (unlike V1 which skips them)
    auto const& times = result->getTimesWithData();
    REQUIRE(times.size() == 1);
    
    auto const& points = result->getAtTime(TimeFrameIndex(10));
    REQUIRE(points.size() == 1);
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

TEST_CASE("TransformsV2 - Centroid: Mixed empty and non-empty masks via scenario",
          "[transforms][v2][centroid][edge][scenario]") {
    auto mask_data = mask_scenarios::mixed_empty_nonempty_centroid();
    auto& registry = ElementRegistry::instance();
    
    MaskCentroidParams params;
    auto transform_fn = registry.getTransformFunction<Mask2D, Point2D<float>, MaskCentroidParams>(
        "CalculateMaskCentroid", params);
    
    auto result = std::make_shared<PointData>();
    for (auto const& [time, entry] : mask_data->elements()) {
        Point2D<float> centroid = transform_fn(entry.data);
        result->addAtTime(time, centroid, NotifyObservers::No);
    }
    result->setTimeFrame(mask_data->getTimeFrame());
    
    auto const& times = result->getTimesWithData();
    REQUIRE(times.size() == 1);
    REQUIRE(*times.begin() == TimeFrameIndex(20));
    
    auto const& points_view = result->getAtTime(TimeFrameIndex(20));
    std::vector<Point2D<float>> points(points_view.begin(), points_view.end());
    
    // V2 includes both masks (empty gives (0,0), non-empty gives real centroid)
    REQUIRE(points.size() == 2);
    
    // Sort by x to get consistent ordering
    std::sort(points.begin(), points.end(),
              [](auto const& a, auto const& b) { return a.x < b.x; });
    
    // Empty mask centroid: (0, 0)
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    
    // Non-empty centroid: (2+4)/2 = 3, (1+3)/2 = 2
    REQUIRE_THAT(points[1].x, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(points[1].y, Catch::Matchers::WithinAbs(2.0f, 0.001f));
}

TEST_CASE("TransformsV2 - Centroid: Single point masks via scenario",
          "[transforms][v2][centroid][edge][scenario]") {
    auto mask_data = mask_scenarios::single_point_masks_centroid();
    auto& registry = ElementRegistry::instance();
    
    MaskCentroidParams params;
    auto transform_fn = registry.getTransformFunction<Mask2D, Point2D<float>, MaskCentroidParams>(
        "CalculateMaskCentroid", params);
    
    auto result = std::make_shared<PointData>();
    for (auto const& [time, entry] : mask_data->elements()) {
        Point2D<float> centroid = transform_fn(entry.data);
        result->addAtTime(time, centroid, NotifyObservers::No);
    }
    result->setTimeFrame(mask_data->getTimeFrame());
    
    auto const& points_view = result->getAtTime(TimeFrameIndex(30));
    std::vector<Point2D<float>> points(points_view.begin(), points_view.end());
    REQUIRE(points.size() == 2);
    
    // Sort points by x coordinate for consistent testing
    auto sorted_points = points;
    std::sort(sorted_points.begin(), sorted_points.end(),
              [](auto const & a, auto const & b) { return a.x < b.x; });
    
    // First mask centroid should be exactly the single point
    REQUIRE_THAT(sorted_points[0].x, Catch::Matchers::WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(sorted_points[0].y, Catch::Matchers::WithinAbs(7.0f, 0.001f));
    
    // Second mask centroid should be exactly the single point
    REQUIRE_THAT(sorted_points[1].x, Catch::Matchers::WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(sorted_points[1].y, Catch::Matchers::WithinAbs(15.0f, 0.001f));
}

TEST_CASE("TransformsV2 - Centroid: Large coordinates via scenario",
          "[transforms][v2][centroid][edge][scenario]") {
    auto mask_data = mask_scenarios::large_coordinates_centroid();
    auto& registry = ElementRegistry::instance();
    
    MaskCentroidParams params;
    auto transform_fn = registry.getTransformFunction<Mask2D, Point2D<float>, MaskCentroidParams>(
        "CalculateMaskCentroid", params);
    
    auto result = std::make_shared<PointData>();
    for (auto const& [time, entry] : mask_data->elements()) {
        Point2D<float> centroid = transform_fn(entry.data);
        result->addAtTime(time, centroid, NotifyObservers::No);
    }
    result->setTimeFrame(mask_data->getTimeFrame());
    
    auto const& points = result->getAtTime(TimeFrameIndex(40));
    REQUIRE(points.size() == 1);
    
    // Centroid: (1000000+1000001+1000002)/3 = 1000001, (2000000+2000001+2000002)/3 = 2000001
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1000001.0f, 0.1f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(2000001.0f, 0.1f));
}

// ============================================================================
// Element-level Transform Tests
// ============================================================================

TEST_CASE("TransformsV2 - Centroid Element Transform", "[transforms][v2][element][centroid]") {
    // Create a simple test mask: square with 4 pixels
    Mask2D mask({
        Point2D<uint32_t>{0, 0},
        Point2D<uint32_t>{2, 0},
        Point2D<uint32_t>{0, 2},
        Point2D<uint32_t>{2, 2}
    });
    // Centroid should be (1, 1)
    
    MaskCentroidParams params;
    auto result = calculateMaskCentroid(mask, params);
    
    REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

TEST_CASE("TransformsV2 - Centroid Empty Mask", "[transforms][v2][element][centroid]") {
    // Empty mask should give centroid of (0, 0)
    Mask2D empty_mask;
    
    MaskCentroidParams params;
    auto result = calculateMaskCentroid(empty_mask, params);
    
    REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

TEST_CASE("TransformsV2 - Centroid Single Point Mask", "[transforms][v2][element][centroid]") {
    // Single point mask should give that point as centroid
    Mask2D single_point({Point2D<uint32_t>{42, 73}});
    
    MaskCentroidParams params;
    auto result = calculateMaskCentroid(single_point, params);
    
    REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(42.0f, 0.001f));
    REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(73.0f, 0.001f));
}

// ============================================================================
// Registry Tests
// ============================================================================

TEST_CASE("TransformsV2 - Centroid Registry Basic Registration", "[transforms][v2][registry][centroid]") {
    auto& registry = ElementRegistry::instance();
    
    // Verify it was registered
    REQUIRE(registry.hasTransform("CalculateMaskCentroid"));
    
    auto const* meta = registry.getMetadata("CalculateMaskCentroid");
    REQUIRE(meta != nullptr);
    REQUIRE(meta->name == "CalculateMaskCentroid");
    REQUIRE(meta->description == "Calculate the centroid (center of mass) of a mask");
}

TEST_CASE("TransformsV2 - Centroid Registry Execute Element Transform", "[transforms][v2][registry][centroid]") {
    auto& registry = ElementRegistry::instance();
    
    // Create test mask: triangle
    Mask2D mask({
        Point2D<uint32_t>{0, 0},
        Point2D<uint32_t>{3, 0},
        Point2D<uint32_t>{0, 3}
    });
    // Centroid: (1, 1)
    
    MaskCentroidParams params;
    
    // Execute via registry
    auto result = registry.execute<Mask2D, Point2D<float>, MaskCentroidParams>(
        "CalculateMaskCentroid", mask, params);
    
    REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

// ============================================================================
// V2 DataManager Integration Tests (using scenarios)
// ============================================================================

TEST_CASE("TransformsV2 - Centroid DataManager JSON load via scenario", 
          "[transforms][v2][datamanager][centroid]") {
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Use scenario data
    auto mask_data = mask_scenarios::json_pipeline_basic_centroid();
    dm.setData("json_pipeline_basic_centroid", mask_data, TimeKey("default"));
    
    // JSON config using scenario's pre-populated data
    nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "centroid_step"},
            {"transform_name", "CalculateMaskCentroid"},
            {"input_key", "json_pipeline_basic_centroid"},
            {"output_key", "v2_calculated_centroids"},
            {"parameters", nlohmann::json::object()}
        }}}
    };
    
    DataManagerPipelineExecutor executor(&dm);
    REQUIRE(executor.loadFromJson(json_config));
    
    auto result = executor.execute();
    REQUIRE(result.success);
    
    auto centroids = dm.getData<PointData>("v2_calculated_centroids");
    REQUIRE(centroids != nullptr);
    
    // Verify data structure
    auto times = centroids->getTimesWithData();
    REQUIRE(times.size() == 3);  // 3 timestamps
    
    // Timestamp 100: Triangle (0,0), (3,0), (0,3) -> centroid at (1, 1)
    auto const& points100 = centroids->getAtTime(TimeFrameIndex(100));
    REQUIRE(points100.size() == 1);
    REQUIRE_THAT(points100[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(points100[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    
    // Timestamp 200: Square (1,1), (3,1), (1,3), (3,3) -> centroid at (2, 2)
    auto const& points200 = centroids->getAtTime(TimeFrameIndex(200));
    REQUIRE(points200.size() == 1);
    REQUIRE_THAT(points200[0].x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(points200[0].y, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    
    // Timestamp 300: Two squares -> centroids at (1, 1) and (6, 6)
    auto const& points300_view = centroids->getAtTime(TimeFrameIndex(300));
    std::vector<Point2D<float>> points300(points300_view.begin(), points300_view.end());
    REQUIRE(points300.size() == 2);
    
    // Sort by x for consistent testing
    std::sort(points300.begin(), points300.end(),
              [](auto const& a, auto const& b) { return a.x < b.x; });
    
    REQUIRE_THAT(points300[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(points300[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(points300[1].x, Catch::Matchers::WithinAbs(6.0f, 0.001f));
    REQUIRE_THAT(points300[1].y, Catch::Matchers::WithinAbs(6.0f, 0.001f));
}

TEST_CASE("TransformsV2 - Centroid DataManager empty mask JSON via scenario", 
          "[transforms][v2][datamanager][centroid]") {
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Use scenario data
    auto mask_data = mask_scenarios::empty_mask_data();
    dm.setData("empty_mask_data", mask_data, TimeKey("default"));
    
    nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "empty_centroid_step"},
            {"transform_name", "CalculateMaskCentroid"},
            {"input_key", "empty_mask_data"},
            {"output_key", "v2_empty_centroids"},
            {"parameters", nlohmann::json::object()}
        }}}
    };
    
    DataManagerPipelineExecutor executor(&dm);
    REQUIRE(executor.loadFromJson(json_config));
    
    auto result = executor.execute();
    REQUIRE(result.success);
    
    auto centroids = dm.getData<PointData>("v2_empty_centroids");
    REQUIRE(centroids != nullptr);
    REQUIRE(centroids->getTimesWithData().empty());
}

TEST_CASE("TransformsV2 - Centroid DataManager operation execute test via scenario", 
          "[transforms][v2][datamanager][centroid]") {
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Use scenario data
    auto mask_data = mask_scenarios::operation_execute_test_centroid();
    dm.setData("operation_execute_test", mask_data, TimeKey("default"));
    
    nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "exec_centroid_step"},
            {"transform_name", "CalculateMaskCentroid"},
            {"input_key", "operation_execute_test"},
            {"output_key", "v2_exec_centroids"},
            {"parameters", nlohmann::json::object()}
        }}}
    };
    
    DataManagerPipelineExecutor executor(&dm);
    REQUIRE(executor.loadFromJson(json_config));
    
    auto result = executor.execute();
    REQUIRE(result.success);
    
    auto centroids = dm.getData<PointData>("v2_exec_centroids");
    REQUIRE(centroids != nullptr);
    
    // Verify: Horizontal line mask (0, 0), (2, 0), (4, 0) -> centroid at (2, 0)
    auto const& points = centroids->getAtTime(TimeFrameIndex(50));
    REQUIRE(points.size() == 1);
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

// ============================================================================
// Context-Aware Transform Tests
// ============================================================================

TEST_CASE("TransformsV2 - Centroid Context-Aware Registration", "[transforms][v2][registry][context][centroid]") {
    auto& registry = ElementRegistry::instance();
    
    // Verify context-aware version is registered
    REQUIRE(registry.hasTransform("CalculateMaskCentroidWithContext"));
    
    auto const* meta = registry.getMetadata("CalculateMaskCentroidWithContext");
    REQUIRE(meta != nullptr);
    REQUIRE(meta->supports_cancellation == true);
}

TEST_CASE("TransformsV2 - Centroid Context-Aware Execute", "[transforms][v2][registry][context][centroid]") {
    // Create test mask
    Mask2D mask({
        Point2D<uint32_t>{0, 0},
        Point2D<uint32_t>{4, 0},
        Point2D<uint32_t>{0, 4},
        Point2D<uint32_t>{4, 4}
    });
    // Centroid: (2, 2)
    
    MaskCentroidParams params;
    
    // Track progress
    int last_progress = -1;
    ComputeContext ctx;
    ctx.progress = [&last_progress](int p) { last_progress = p; };
    ctx.is_cancelled = []() { return false; };
    
    auto result = calculateMaskCentroidWithContext(mask, params, ctx);
    
    REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    REQUIRE(last_progress == 100);  // Progress should reach 100%
}
