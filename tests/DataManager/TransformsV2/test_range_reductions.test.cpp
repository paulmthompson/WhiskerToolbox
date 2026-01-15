/**
 * @file test_range_reductions.test.cpp
 * @brief Tests for range reduction algorithms
 *
 * These tests verify:
 * 1. Event range reductions (EventCount, FirstPositiveLatency, etc.)
 * 2. Value range reductions (MaxValue, TimeOfMax, etc.)
 * 3. Edge cases (empty ranges, NaN handling)
 * 4. Registry registration and execution
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "transforms/v2/algorithms/RangeReductions/EventRangeReductions.hpp"
#include "transforms/v2/algorithms/RangeReductions/RegisteredRangeReductions.hpp"
#include "transforms/v2/algorithms/RangeReductions/ValueRangeReductions.hpp"
#include "transforms/v2/core/RangeReductionRegistry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2::RangeReductions;
using namespace WhiskerToolbox::Transforms::V2;

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * @brief Create test events with specified times
 */
inline std::vector<EventWithId> makeEvents(std::vector<int> const & times) {
    std::vector<EventWithId> events;
    events.reserve(times.size());
    for (size_t i = 0; i < times.size(); ++i) {
        events.emplace_back(TimeFrameIndex{times[i]}, EntityId{static_cast<uint64_t>(i + 1)});
    }
    return events;
}

/**
 * @brief Create test value points with specified time-value pairs
 */
inline std::vector<AnalogTimeSeries::TimeValuePoint> makePoints(
        std::vector<std::pair<int, float>> const & data) {
    std::vector<AnalogTimeSeries::TimeValuePoint> points;
    points.reserve(data.size());
    for (auto const & [time, value] : data) {
        points.emplace_back(TimeFrameIndex{time}, value);
    }
    return points;
}

// ============================================================================
// Event Range Reduction Tests
// ============================================================================

TEST_CASE("EventCount - counts events correctly", "[RangeReductions][Event]") {
    SECTION("Multiple events") {
        auto events = makeEvents({-50, -10, 25, 100, 200});
        auto span = std::span<EventWithId const>{events};
        CHECK(eventCount(span) == 5);
    }

    SECTION("Single event") {
        auto events = makeEvents({42});
        auto span = std::span<EventWithId const>{events};
        CHECK(eventCount(span) == 1);
    }

    SECTION("Empty range") {
        std::vector<EventWithId> events;
        auto span = std::span<EventWithId const>{events};
        CHECK(eventCount(span) == 0);
    }
}

TEST_CASE("FirstPositiveLatency - finds first positive time", "[RangeReductions][Event]") {
    SECTION("Events spanning zero") {
        auto events = makeEvents({-50, -10, 25, 100, 200});
        auto span = std::span<EventWithId const>{events};
        CHECK_THAT(firstPositiveLatency(span), Catch::Matchers::WithinRel(25.0f, 0.001f));
    }

    SECTION("All negative") {
        auto events = makeEvents({-100, -50, -10});
        auto span = std::span<EventWithId const>{events};
        CHECK(std::isnan(firstPositiveLatency(span)));
    }

    SECTION("All positive") {
        auto events = makeEvents({10, 50, 100});
        auto span = std::span<EventWithId const>{events};
        CHECK_THAT(firstPositiveLatency(span), Catch::Matchers::WithinRel(10.0f, 0.001f));
    }

    SECTION("Empty range") {
        std::vector<EventWithId> events;
        auto span = std::span<EventWithId const>{events};
        CHECK(std::isnan(firstPositiveLatency(span)));
    }

    SECTION("Event at exactly zero is not positive") {
        auto events = makeEvents({-10, 0, 10});
        auto span = std::span<EventWithId const>{events};
        CHECK_THAT(firstPositiveLatency(span), Catch::Matchers::WithinRel(10.0f, 0.001f));
    }
}

TEST_CASE("LastNegativeLatency - finds last negative time", "[RangeReductions][Event]") {
    SECTION("Events spanning zero") {
        auto events = makeEvents({-50, -10, 25, 100});
        auto span = std::span<EventWithId const>{events};
        CHECK_THAT(lastNegativeLatency(span), Catch::Matchers::WithinRel(-10.0f, 0.001f));
    }

    SECTION("All positive") {
        auto events = makeEvents({10, 50, 100});
        auto span = std::span<EventWithId const>{events};
        CHECK(std::isnan(lastNegativeLatency(span)));
    }

    SECTION("All negative") {
        auto events = makeEvents({-100, -50, -10});
        auto span = std::span<EventWithId const>{events};
        CHECK_THAT(lastNegativeLatency(span), Catch::Matchers::WithinRel(-10.0f, 0.001f));
    }

    SECTION("Empty range") {
        std::vector<EventWithId> events;
        auto span = std::span<EventWithId const>{events};
        CHECK(std::isnan(lastNegativeLatency(span)));
    }
}

TEST_CASE("EventCountInWindow - counts events in time window", "[RangeReductions][Event]") {
    auto events = makeEvents({-50, -10, 0, 25, 50, 100, 200});

    SECTION("Count positive events only") {
        auto span = std::span<EventWithId const>{events};
        TimeWindowParams params{.window_start = 0.0f, .window_end = 100.0f};
        // Events: 0, 25, 50 are in [0, 100)
        CHECK(eventCountInWindow(span, params) == 3);
    }

    SECTION("Count all events") {
        auto span = std::span<EventWithId const>{events};
        TimeWindowParams params{
                .window_start = -100.0f,
                .window_end = 300.0f};
        CHECK(eventCountInWindow(span, params) == 7);
    }

    SECTION("Empty window") {
        auto span = std::span<EventWithId const>{events};
        TimeWindowParams params{.window_start = 300.0f, .window_end = 400.0f};
        CHECK(eventCountInWindow(span, params) == 0);
    }
}

TEST_CASE("MeanInterEventInterval - computes mean interval", "[RangeReductions][Event]") {
    SECTION("Regular intervals") {
        auto events = makeEvents({0, 10, 20, 30});
        auto span = std::span<EventWithId const>{events};
        // Intervals: 10, 10, 10 → mean = 10
        CHECK_THAT(meanInterEventInterval(span), Catch::Matchers::WithinRel(10.0f, 0.001f));
    }

    SECTION("Irregular intervals") {
        auto events = makeEvents({0, 10, 30, 60});
        auto span = std::span<EventWithId const>{events};
        // Intervals: 10, 20, 30 → mean = 20
        CHECK_THAT(meanInterEventInterval(span), Catch::Matchers::WithinRel(20.0f, 0.001f));
    }

    SECTION("Single event") {
        auto events = makeEvents({42});
        auto span = std::span<EventWithId const>{events};
        CHECK(std::isnan(meanInterEventInterval(span)));
    }

    SECTION("Empty range") {
        std::vector<EventWithId> events;
        auto span = std::span<EventWithId const>{events};
        CHECK(std::isnan(meanInterEventInterval(span)));
    }
}

TEST_CASE("EventTimeSpan - computes time span", "[RangeReductions][Event]") {
    SECTION("Multiple events") {
        auto events = makeEvents({-50, 0, 100, 200});
        auto span = std::span<EventWithId const>{events};
        // Span: 200 - (-50) = 250
        CHECK_THAT(eventTimeSpan(span), Catch::Matchers::WithinRel(250.0f, 0.001f));
    }

    SECTION("Single event") {
        auto events = makeEvents({42});
        auto span = std::span<EventWithId const>{events};
        CHECK_THAT(eventTimeSpan(span), Catch::Matchers::WithinRel(0.0f, 0.001f));
    }

    SECTION("Empty range") {
        std::vector<EventWithId> events;
        auto span = std::span<EventWithId const>{events};
        CHECK_THAT(eventTimeSpan(span), Catch::Matchers::WithinRel(0.0f, 0.001f));
    }
}

// ============================================================================
// Value Range Reduction Tests
// ============================================================================

TEST_CASE("MaxValue - finds maximum value", "[RangeReductions][Value]") {
    SECTION("Multiple values") {
        auto points = makePoints({{0, 1.0f}, {10, 5.0f}, {20, 3.0f}, {30, 2.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(maxValue(span), Catch::Matchers::WithinRel(5.0f, 0.001f));
    }

    SECTION("Negative values") {
        auto points = makePoints({{0, -5.0f}, {10, -2.0f}, {20, -10.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(maxValue(span), Catch::Matchers::WithinRel(-2.0f, 0.001f));
    }

    SECTION("Empty range") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points;
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK(maxValue(span) == -std::numeric_limits<float>::infinity());
    }
}

TEST_CASE("MinValue - finds minimum value", "[RangeReductions][Value]") {
    SECTION("Multiple values") {
        auto points = makePoints({{0, 1.0f}, {10, 5.0f}, {20, 3.0f}, {30, 2.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(minValue(span), Catch::Matchers::WithinRel(1.0f, 0.001f));
    }

    SECTION("Empty range") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points;
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK(minValue(span) == std::numeric_limits<float>::infinity());
    }
}

TEST_CASE("MeanValue - computes mean value", "[RangeReductions][Value]") {
    SECTION("Simple mean") {
        auto points = makePoints({{0, 2.0f}, {10, 4.0f}, {20, 6.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(meanValue(span), Catch::Matchers::WithinRel(4.0f, 0.001f));
    }

    SECTION("Single value") {
        auto points = makePoints({{0, 42.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(meanValue(span), Catch::Matchers::WithinRel(42.0f, 0.001f));
    }

    SECTION("Empty range") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points;
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK(std::isnan(meanValue(span)));
    }
}

TEST_CASE("StdValue - computes standard deviation", "[RangeReductions][Value]") {
    SECTION("Uniform values") {
        auto points = makePoints({{0, 5.0f}, {10, 5.0f}, {20, 5.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(stdValue(span), Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    }

    SECTION("Known standard deviation") {
        // Values: 2, 4, 4, 4, 5, 5, 7, 9 → mean = 5, population std ≈ 2.0
        auto points = makePoints({
                {0, 2.0f},
                {1, 4.0f},
                {2, 4.0f},
                {3, 4.0f},
                {4, 5.0f},
                {5, 5.0f},
                {6, 7.0f},
                {7, 9.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(stdValue(span), Catch::Matchers::WithinRel(2.0f, 0.01f));
    }

    SECTION("Single value") {
        auto points = makePoints({{0, 42.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(stdValue(span), Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    }

    SECTION("Empty range") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points;
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK(std::isnan(stdValue(span)));
    }
}

TEST_CASE("TimeOfMax - finds time of maximum value", "[RangeReductions][Value]") {
    SECTION("Peak in middle") {
        auto points = makePoints({{0, 1.0f}, {10, 5.0f}, {20, 3.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(timeOfMax(span), Catch::Matchers::WithinRel(10.0f, 0.001f));
    }

    SECTION("Peak at start") {
        auto points = makePoints({{0, 10.0f}, {10, 5.0f}, {20, 3.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(timeOfMax(span), Catch::Matchers::WithinRel(0.0f, 0.001f));
    }

    SECTION("Peak at end") {
        auto points = makePoints({{0, 1.0f}, {10, 5.0f}, {20, 10.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(timeOfMax(span), Catch::Matchers::WithinRel(20.0f, 0.001f));
    }

    SECTION("Empty range") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points;
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK(std::isnan(timeOfMax(span)));
    }
}

TEST_CASE("TimeOfMin - finds time of minimum value", "[RangeReductions][Value]") {
    SECTION("Trough in middle") {
        auto points = makePoints({{0, 5.0f}, {10, 1.0f}, {20, 3.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(timeOfMin(span), Catch::Matchers::WithinRel(10.0f, 0.001f));
    }

    SECTION("Empty range") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points;
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK(std::isnan(timeOfMin(span)));
    }
}

TEST_CASE("TimeOfThresholdCross - detects threshold crossing", "[RangeReductions][Value]") {
    SECTION("Rising crossing") {
        auto points = makePoints({{0, 0.5f}, {10, 1.5f}, {20, 2.5f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        ThresholdCrossParams params{.threshold = 1.0f, .rising = true};
        CHECK_THAT(timeOfThresholdCross(span, params), Catch::Matchers::WithinRel(10.0f, 0.001f));
    }

    SECTION("Falling crossing") {
        auto points = makePoints({{0, 2.5f}, {10, 1.5f}, {20, 0.5f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        ThresholdCrossParams params{.threshold = 1.0f, .rising = false};
        CHECK_THAT(timeOfThresholdCross(span, params), Catch::Matchers::WithinRel(20.0f, 0.001f));
    }

    SECTION("No crossing") {
        auto points = makePoints({{0, 0.5f}, {10, 0.7f}, {20, 0.9f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        ThresholdCrossParams params{.threshold = 1.0f, .rising = true};
        CHECK(std::isnan(timeOfThresholdCross(span, params)));
    }

    SECTION("Too few points") {
        auto points = makePoints({{0, 0.5f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        ThresholdCrossParams params{.threshold = 1.0f, .rising = true};
        CHECK(std::isnan(timeOfThresholdCross(span, params)));
    }
}

TEST_CASE("SumValue - computes sum of values", "[RangeReductions][Value]") {
    SECTION("Multiple values") {
        auto points = makePoints({{0, 1.0f}, {10, 2.0f}, {20, 3.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(sumValue(span), Catch::Matchers::WithinRel(6.0f, 0.001f));
    }

    SECTION("Empty range") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points;
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(sumValue(span), Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    }
}

TEST_CASE("ValueRange - computes max minus min", "[RangeReductions][Value]") {
    SECTION("Multiple values") {
        auto points = makePoints({{0, 1.0f}, {10, 5.0f}, {20, 3.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(valueRange(span), Catch::Matchers::WithinRel(4.0f, 0.001f));
    }

    SECTION("Uniform values") {
        auto points = makePoints({{0, 5.0f}, {10, 5.0f}, {20, 5.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(valueRange(span), Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    }

    SECTION("Empty range") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points;
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK(std::isnan(valueRange(span)));
    }
}

TEST_CASE("AreaUnderCurve - computes trapezoidal integration", "[RangeReductions][Value]") {
    SECTION("Rectangular area") {
        // Constant value 2.0 from t=0 to t=10 → area = 2 * 10 = 20
        auto points = makePoints({{0, 2.0f}, {10, 2.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(areaUnderCurve(span), Catch::Matchers::WithinRel(20.0f, 0.001f));
    }

    SECTION("Triangular area") {
        // From (0, 0) to (10, 10) → area = 0.5 * 10 * 10 = 50
        auto points = makePoints({{0, 0.0f}, {10, 10.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(areaUnderCurve(span), Catch::Matchers::WithinRel(50.0f, 0.001f));
    }

    SECTION("Single point") {
        auto points = makePoints({{0, 5.0f}});
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(areaUnderCurve(span), Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    }

    SECTION("Empty range") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points;
        auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};
        CHECK_THAT(areaUnderCurve(span), Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    }
}

TEST_CASE("CountAboveThreshold - counts samples above threshold", "[RangeReductions][Value]") {
    auto points = makePoints({{0, 1.0f}, {10, 2.0f}, {20, 3.0f}, {30, 4.0f}});
    auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};

    SECTION("Some above") {
        ThresholdCrossParams params{.threshold = 2.0f};
        CHECK(countAboveThreshold(span, params) == 2);// 3.0 and 4.0
    }

    SECTION("None above") {
        ThresholdCrossParams params{.threshold = 10.0f};
        CHECK(countAboveThreshold(span, params) == 0);
    }

    SECTION("All above") {
        ThresholdCrossParams params{.threshold = 0.0f};
        CHECK(countAboveThreshold(span, params) == 4);
    }
}

TEST_CASE("FractionAboveThreshold - computes fraction above threshold", "[RangeReductions][Value]") {
    auto points = makePoints({{0, 1.0f}, {10, 2.0f}, {20, 3.0f}, {30, 4.0f}});
    auto span = std::span<AnalogTimeSeries::TimeValuePoint const>{points};

    SECTION("Half above") {
        ThresholdCrossParams params{.threshold = 2.0f};
        CHECK_THAT(fractionAboveThreshold(span, params), Catch::Matchers::WithinRel(0.5f, 0.001f));
    }

    SECTION("Empty range") {
        std::vector<AnalogTimeSeries::TimeValuePoint> empty_points;
        auto empty_span = std::span<AnalogTimeSeries::TimeValuePoint const>{empty_points};
        ThresholdCrossParams params{.threshold = 0.0f};
        CHECK(std::isnan(fractionAboveThreshold(empty_span, params)));
    }
}

// ============================================================================
// Registry Integration Tests
// ============================================================================

TEST_CASE("Registry - Event reductions are registered", "[RangeReductions][Registry]") {
    auto & registry = RangeReductionRegistry::instance();

    SECTION("EventCount is registered") {
        REQUIRE(registry.hasReduction("EventCount"));
        auto const * meta = registry.getMetadata("EventCount");
        REQUIRE(meta != nullptr);
        CHECK(meta->category == "Event Statistics");
        CHECK(meta->input_type == std::type_index(typeid(EventWithId)));
        CHECK(meta->output_type == std::type_index(typeid(int)));
    }

    SECTION("FirstPositiveLatency is registered") {
        REQUIRE(registry.hasReduction("FirstPositiveLatency"));
        auto const * meta = registry.getMetadata("FirstPositiveLatency");
        REQUIRE(meta != nullptr);
        CHECK(meta->output_type == std::type_index(typeid(float)));
    }

    SECTION("EventCountInWindow is registered with parameters") {
        REQUIRE(registry.hasReduction("EventCountInWindow"));
        auto const * meta = registry.getMetadata("EventCountInWindow");
        REQUIRE(meta != nullptr);
        CHECK(meta->params_type == std::type_index(typeid(TimeWindowParams)));
    }
}

TEST_CASE("Registry - Value reductions are registered", "[RangeReductions][Registry]") {
    auto & registry = RangeReductionRegistry::instance();

    using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;

    SECTION("MaxValue is registered") {
        REQUIRE(registry.hasReduction("MaxValue"));
        auto const * meta = registry.getMetadata("MaxValue");
        REQUIRE(meta != nullptr);
        CHECK(meta->category == "Value Statistics");
        CHECK(meta->input_type == std::type_index(typeid(TimeValuePoint)));
        CHECK(meta->output_type == std::type_index(typeid(float)));
    }

    SECTION("TimeOfThresholdCross is registered with parameters") {
        REQUIRE(registry.hasReduction("TimeOfThresholdCross"));
        auto const * meta = registry.getMetadata("TimeOfThresholdCross");
        REQUIRE(meta != nullptr);
        CHECK(meta->params_type == std::type_index(typeid(ThresholdCrossParams)));
    }
}

TEST_CASE("Registry - Discovery API works", "[RangeReductions][Registry]") {
    auto & registry = RangeReductionRegistry::instance();

    SECTION("Get reductions for EventWithId") {
        auto names = registry.getReductionsForInputType<EventWithId>();
        CHECK(!names.empty());
        CHECK(std::find(names.begin(), names.end(), "EventCount") != names.end());
        CHECK(std::find(names.begin(), names.end(), "FirstPositiveLatency") != names.end());
    }

    SECTION("Get reductions for TimeValuePoint") {
        using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;
        auto names = registry.getReductionsForInputType<TimeValuePoint>();
        CHECK(!names.empty());
        CHECK(std::find(names.begin(), names.end(), "MaxValue") != names.end());
        CHECK(std::find(names.begin(), names.end(), "TimeOfMax") != names.end());
    }
}

TEST_CASE("Registry - Type-safe execution works", "[RangeReductions][Registry]") {
    auto & registry = RangeReductionRegistry::instance();

    SECTION("Execute EventCount") {
        auto events = makeEvents({-50, -10, 25, 100, 200});
        auto span = std::span<EventWithId const>{events};
        auto result = registry.execute<EventWithId, int, NoReductionParams>(
                "EventCount", span, NoReductionParams{});
        CHECK(result == 5);
    }

    SECTION("Execute MaxValue") {
        using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;
        auto points = makePoints({{0, 1.0f}, {10, 5.0f}, {20, 3.0f}});
        auto span = std::span<TimeValuePoint const>{points};
        auto result = registry.execute<TimeValuePoint, float, NoReductionParams>(
                "MaxValue", span, NoReductionParams{});
        CHECK_THAT(result, Catch::Matchers::WithinRel(5.0f, 0.001f));
    }

    SECTION("Execute parameterized reduction") {
        auto events = makeEvents({-50, -10, 0, 25, 50, 100});
        auto span = std::span<EventWithId const>{events};
        TimeWindowParams params{.window_start = 0.0f, .window_end = 100.0f};
        auto result = registry.execute<EventWithId, int, TimeWindowParams>(
                "EventCountInWindow", span, params);
        CHECK(result == 3);// 0, 25, 50 are in [0, 100)
    }
}
