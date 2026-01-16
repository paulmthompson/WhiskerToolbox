#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/detail/FlatZipView.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <nlohmann/json.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include <cmath>
#include <memory>
#include <ranges>
#include <tuple>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Test Fixtures and Helpers
// ============================================================================

namespace {

// Helper to create LineData with known geometry for distance testing
std::shared_ptr<LineData> createTestLineData() {
    auto line_data = std::make_shared<LineData>();

    // T=0: Horizontal line at y=0 from x=0 to x=10
    Line2D line1;
    line1.push_back(Point2D<float>{0.0f, 0.0f});
    line1.push_back(Point2D<float>{10.0f, 0.0f});
    line_data->addAtTime(TimeFrameIndex(0), std::move(line1), NotifyObservers::No);

    // T=1: Horizontal line at y=0 from x=0 to x=10
    Line2D line2;
    line2.push_back(Point2D<float>{0.0f, 0.0f});
    line2.push_back(Point2D<float>{10.0f, 0.0f});
    line_data->addAtTime(TimeFrameIndex(1), std::move(line2), NotifyObservers::No);

    // T=2: Horizontal line at y=0 from x=0 to x=10
    Line2D line3;
    line3.push_back(Point2D<float>{0.0f, 0.0f});
    line3.push_back(Point2D<float>{10.0f, 0.0f});
    line_data->addAtTime(TimeFrameIndex(2), std::move(line3), NotifyObservers::No);

    return line_data;
}

// Helper to create PointData with known positions for distance testing
std::shared_ptr<PointData> createTestPointData() {
    auto point_data = std::make_shared<PointData>();

    // T=0: Point at (5, 3) -> distance to line at y=0 is 3.0
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{5.0f, 3.0f}, NotifyObservers::No);

    // T=1: Point at (5, 4) -> distance to line at y=0 is 4.0
    point_data->addAtTime(TimeFrameIndex(1), Point2D<float>{5.0f, 4.0f}, NotifyObservers::No);

    // T=2: Point at (5, 5) -> distance to line at y=0 is 5.0
    point_data->addAtTime(TimeFrameIndex(2), Point2D<float>{5.0f, 5.0f}, NotifyObservers::No);

    return point_data;
}

// Helper to create PointData with multiple points per time frame
std::shared_ptr<PointData> createMultiPointData() {
    auto point_data = std::make_shared<PointData>();

    // T=0: Two points at y=3 and y=5 -> distances 3.0 and 5.0, sum = 8.0
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{5.0f, 3.0f}, NotifyObservers::No);
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{5.0f, 5.0f}, NotifyObservers::No);

    // T=1: Two points at y=2 and y=4 -> distances 2.0 and 4.0, sum = 6.0
    point_data->addAtTime(TimeFrameIndex(1), Point2D<float>{5.0f, 2.0f}, NotifyObservers::No);
    point_data->addAtTime(TimeFrameIndex(1), Point2D<float>{5.0f, 4.0f}, NotifyObservers::No);

    return point_data;
}

// Create a TimeFrame for testing
std::shared_ptr<TimeFrame> createTestTimeFrame() {
    return std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2, 3, 4});
}

}// anonymous namespace

// ============================================================================
// Basic Multi-Input Step Execution Tests
// ============================================================================

TEST_CASE("DataManagerPipelineExecutor - Multi-input step descriptor", "[transforms][v2][pipeline][multi-input]") {

    SECTION("Step descriptor correctly identifies multi-input") {
        DataManagerStepDescriptor step;
        step.step_id = "1";
        step.transform_name = "TestTransform";
        step.input_key = "data1";

        REQUIRE_FALSE(step.isMultiInput());

        step.additional_input_keys = std::vector<std::string>{"data2"};
        REQUIRE(step.isMultiInput());

        auto all_keys = step.getAllInputKeys();
        REQUIRE(all_keys.size() == 2);
        REQUIRE(all_keys[0] == "data1");
        REQUIRE(all_keys[1] == "data2");
    }

    SECTION("Empty additional_input_keys is not multi-input") {
        DataManagerStepDescriptor step;
        step.step_id = "1";
        step.transform_name = "TestTransform";
        step.input_key = "data1";
        step.additional_input_keys = std::vector<std::string>{};

        REQUIRE_FALSE(step.isMultiInput());
    }
}

TEST_CASE("DataManagerPipelineExecutor - Binary transform via JSON", "[transforms][v2][pipeline][multi-input][integration]") {

    // Setup DataManager with test data
    DataManager dm;
    auto time_frame = createTestTimeFrame();
    dm.setTime(TimeKey("default"), time_frame);

    auto lines = createTestLineData();
    lines->setTimeFrame(time_frame);
    dm.setData<LineData>("lines", lines, TimeKey("default"));

    auto points = createTestPointData();
    points->setTimeFrame(time_frame);
    dm.setData<PointData>("points", points, TimeKey("default"));

    SECTION("Execute binary transform with JSON configuration") {
        nlohmann::json pipeline_json = {
                {"steps", {{{"step_id", "calculate_distance"}, {"transform_name", "CalculateLineMinPointDistance"}, {"input_key", "lines"}, {"additional_input_keys", {"points"}}, {"output_key", "distances"}, {"parameters", {}}}}}};

        DataManagerPipelineExecutor executor(&dm);
        REQUIRE(executor.loadFromJson(pipeline_json));

        auto result = executor.execute();
        REQUIRE(result.success);
        REQUIRE(result.steps_completed == 1);

        // Verify output was stored
        auto distances = dm.getData<RaggedAnalogTimeSeries>("distances");
        REQUIRE(distances != nullptr);

        // Check calculated distances
        // T=0: point at (5,3), line at y=0 -> distance = 3.0
        // T=1: point at (5,4), line at y=0 -> distance = 4.0
        // T=2: point at (5,5), line at y=0 -> distance = 5.0
        auto time_indices = distances->getTimeIndices();
        REQUIRE(time_indices.size() == 3);

        auto data_t0 = distances->getDataAtTime(TimeFrameIndex(0));
        REQUIRE(data_t0.size() == 1);
        REQUIRE_THAT(data_t0[0], WithinAbs(3.0f, 0.001f));

        auto data_t1 = distances->getDataAtTime(TimeFrameIndex(1));
        REQUIRE(data_t1.size() == 1);
        REQUIRE_THAT(data_t1[0], WithinAbs(4.0f, 0.001f));

        auto data_t2 = distances->getDataAtTime(TimeFrameIndex(2));
        REQUIRE(data_t2.size() == 1);
        REQUIRE_THAT(data_t2[0], WithinAbs(5.0f, 0.001f));
    }
}

// ============================================================================
// Element Transform Fusion Tests
// ============================================================================

TEST_CASE("DataManagerPipelineExecutor - Multi-input fusion analysis", "[transforms][v2][pipeline][multi-input][fusion]") {

    // This test verifies that the fusion analysis correctly identifies fusible steps
    // after a multi-input transform

    DataManager dm;
    auto time_frame = createTestTimeFrame();
    dm.setTime(TimeKey("default"), time_frame);

    auto lines = createTestLineData();
    lines->setTimeFrame(time_frame);
    dm.setData<LineData>("lines", lines, TimeKey("default"));

    auto points = createTestPointData();
    points->setTimeFrame(time_frame);
    dm.setData<PointData>("points", points, TimeKey("default"));

    SECTION("Single binary transform executes successfully") {
        // Just test that the binary transform alone works
        nlohmann::json pipeline_json = {
                {"steps", {{{"step_id", "1"}, {"transform_name", "CalculateLineMinPointDistance"}, {"input_key", "lines"}, {"additional_input_keys", {"points"}}, {"output_key", "distances"}}}}};

        DataManagerPipelineExecutor executor(&dm);
        REQUIRE(executor.loadFromJson(pipeline_json));

        auto result = executor.execute();
        REQUIRE(result.success);
        REQUIRE(result.steps_completed == 1);

        auto distances = dm.getData<RaggedAnalogTimeSeries>("distances");
        REQUIRE(distances != nullptr);

        auto data_t0 = distances->getDataAtTime(TimeFrameIndex(0));
        REQUIRE(data_t0.size() >= 1);
        REQUIRE_THAT(data_t0[0], WithinAbs(3.0f, 0.001f));
    }

    SECTION("Multi-input step is correctly identified as non-fusible") {
        DataManagerPipelineExecutor executor(&dm);

        nlohmann::json pipeline_json = {
                {"steps", {{{"step_id", "1"}, {"transform_name", "CalculateLineMinPointDistance"}, {"input_key", "lines"}, {"additional_input_keys", {"points"}}, {"output_key", "distances"}}}}};

        REQUIRE(executor.loadFromJson(pipeline_json));

        // Multi-input steps cannot be fused with previous steps
        REQUIRE_FALSE(executor.canFuseStep(0));
    }
}

// ============================================================================
// Step Chaining Detection Tests
// ============================================================================

TEST_CASE("DataManagerPipelineExecutor - Step chaining detection", "[transforms][v2][pipeline][chaining]") {
    DataManager dm;
    DataManagerPipelineExecutor executor(&dm);

    SECTION("canFuseStep returns false for multi-input steps") {
        nlohmann::json pipeline_json = {
                {"steps", {{{"step_id", "1"}, {"transform_name", "CalculateLineMinPointDistance"}, {"input_key", "lines"}, {"additional_input_keys", {"points"}}}}}};

        executor.loadFromJson(pipeline_json);

        // Multi-input steps start new segments, so they aren't "fused with previous"
        REQUIRE_FALSE(executor.canFuseStep(0));// First step can't be fused (nothing before it)
    }

    SECTION("canFuseStep returns true for element transforms") {
        auto & registry = ElementRegistry::instance();

        // Ensure ScaleValue is registered (might be from previous test)
        struct ScaleParams {
            std::optional<float> factor;
            float getFactor() const { return factor.value_or(2.0f); }
        };

        if (!registry.hasTransform("ScaleValue")) {
            registry.registerTransform<float, float, ScaleParams>(
                    "ScaleValue",
                    [](float const & val, ScaleParams const & params) {
                        return val * params.getFactor();
                    });
        }

        nlohmann::json pipeline_json = {
                {"steps", {{{"step_id", "1"}, {"transform_name", "CalculateMaskArea"}, {"input_key", "masks"}}, {{"step_id", "2"}, {"transform_name", "ScaleValue"}, {"input_key", "1"}}}}};

        executor.loadFromJson(pipeline_json);

        // Element transforms can be fused
        REQUIRE(executor.canFuseStep(1));
    }

    SECTION("stepsAreChained detects correct chaining") {
        nlohmann::json pipeline_json = {
                {"steps", {{{"step_id", "1"}, {"transform_name", "TransformA"}, {"input_key", "input"}, {"output_key", "intermediate"}}, {
                                                                                                                                                 {"step_id", "2"},
                                                                                                                                                 {"transform_name", "TransformB"},
                                                                                                                                                 {"input_key", "intermediate"}// Chains to step 1's output
                                                                                                                                         },
                           {
                                   {"step_id", "3"},
                                   {"transform_name", "TransformC"},
                                   {"input_key", "different"}// Does NOT chain
                           }}}};

        executor.loadFromJson(pipeline_json);

        REQUIRE(executor.stepsAreChained(0, 1));      // Step 2 uses step 1's output
        REQUIRE_FALSE(executor.stepsAreChained(1, 2));// Step 3 uses different input
    }

    SECTION("stepsAreChained with step_id as implicit output") {
        nlohmann::json pipeline_json = {
                {"steps", {{
                                   {"step_id", "calc_area"},
                                   {"transform_name", "TransformA"},
                                   {"input_key", "input"}// No output_key -> uses step_id "calc_area"
                           },
                           {
                                   {"step_id", "2"},
                                   {"transform_name", "TransformB"},
                                   {"input_key", "calc_area"}// Chains to step 1's step_id
                           }}}};

        executor.loadFromJson(pipeline_json);

        REQUIRE(executor.stepsAreChained(0, 1));
    }
}

// ============================================================================
// Segment Building Tests
// ============================================================================

TEST_CASE("DataManagerPipelineExecutor - Segment building", "[transforms][v2][pipeline][segments]") {
    DataManager dm;
    DataManagerPipelineExecutor executor(&dm);

    SECTION("Single step creates single segment") {
        nlohmann::json pipeline_json = {
                {"steps", {{{"step_id", "1"}, {"transform_name", "CalculateMaskArea"}, {"input_key", "masks"}, {"output_key", "areas"}}}}};

        executor.loadFromJson(pipeline_json);
        auto segments = executor.buildSegments();

        REQUIRE(segments.size() == 1);
        REQUIRE(segments[0].start_step == 0);
        REQUIRE(segments[0].end_step == 1);
        REQUIRE_FALSE(segments[0].is_multi_input);
    }

    SECTION("Multi-input step creates multi-input segment") {
        nlohmann::json pipeline_json = {
                {"steps", {{{"step_id", "1"}, {"transform_name", "CalculateLineMinPointDistance"}, {"input_key", "lines"}, {"additional_input_keys", {"points"}}, {"output_key", "distances"}}}}};

        executor.loadFromJson(pipeline_json);
        auto segments = executor.buildSegments();

        REQUIRE(segments.size() == 1);
        REQUIRE(segments[0].is_multi_input);
        REQUIRE(segments[0].input_keys.size() == 2);
        REQUIRE(segments[0].input_keys[0] == "lines");
        REQUIRE(segments[0].input_keys[1] == "points");
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("DataManagerPipelineExecutor - Error handling for multi-input", "[transforms][v2][pipeline][multi-input][errors]") {
    DataManager dm;
    auto time_frame = createTestTimeFrame();
    dm.setTime(TimeKey("default"), time_frame);

    SECTION("Missing second input fails gracefully") {
        auto lines = createTestLineData();
        lines->setTimeFrame(time_frame);
        dm.setData<LineData>("lines", lines, TimeKey("default"));
        // Note: "points" is NOT added to DataManager

        nlohmann::json pipeline_json = {
                {"steps", {{{"step_id", "1"}, {"transform_name", "CalculateLineMinPointDistance"}, {"input_key", "lines"}, {"additional_input_keys", {"points"}},// This doesn't exist!
                            {"output_key", "distances"}}}}};

        DataManagerPipelineExecutor executor(&dm);
        REQUIRE(executor.loadFromJson(pipeline_json));

        auto result = executor.execute();
        REQUIRE_FALSE(result.success);
        // Error message should mention the failing step or input
        REQUIRE_FALSE(result.error_message.empty());
    }

    SECTION("Missing first input fails gracefully") {
        auto points = createTestPointData();
        points->setTimeFrame(time_frame);
        dm.setData<PointData>("points", points, TimeKey("default"));
        // Note: "lines" is NOT added to DataManager

        nlohmann::json pipeline_json = {
                {"steps", {{{"step_id", "1"}, {"transform_name", "CalculateLineMinPointDistance"}, {"input_key", "lines"},// This doesn't exist!
                            {"additional_input_keys", {"points"}},
                            {"output_key", "distances"}}}}};

        DataManagerPipelineExecutor executor(&dm);
        REQUIRE(executor.loadFromJson(pipeline_json));

        auto result = executor.execute();
        REQUIRE_FALSE(result.success);
    }
}
