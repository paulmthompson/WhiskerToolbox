#include "transforms/v2/algorithms/RangeReductions/RegisteredRangeReductions.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "transforms/v2/algorithms/RangeReductions/EventRangeReductions.hpp"
#include "transforms/v2/algorithms/RangeReductions/ValueRangeReductions.hpp"
#include "transforms/v2/core/RangeReductionRegistry.hpp"

namespace WhiskerToolbox::Transforms::V2::RangeReductions {

// ============================================================================
// Initialization Function
// ============================================================================

void registerAllRangeReductions() {
    registerEventRangeReductions();
    registerValueRangeReductions();
}

// ============================================================================
// Event Range Reductions Registration
// ============================================================================

void registerEventRangeReductions() {
    auto & registry = RangeReductionRegistry::instance();

    // EventCount - stateless
    registry.registerStatelessReduction<EventWithId, int>(
            "EventCount",
            [](std::span<EventWithId const> events) -> int {
                return eventCount(events);
            },
            RangeReductionMetadata{
                    .description = "Count total number of events in range",
                    .category = "Event Statistics",
                    .requires_time_series_element = true,
                    .requires_entity_element = true});

    // FirstPositiveLatency - stateless
    registry.registerStatelessReduction<EventWithId, float>(
            "FirstPositiveLatency",
            [](std::span<EventWithId const> events) -> float {
                return firstPositiveLatency(events);
            },
            RangeReductionMetadata{
                    .description = "Time of first event with t > 0 (after alignment)",
                    .category = "Event Statistics",
                    .requires_time_series_element = true,
                    .requires_entity_element = true});

    // LastNegativeLatency - stateless
    registry.registerStatelessReduction<EventWithId, float>(
            "LastNegativeLatency",
            [](std::span<EventWithId const> events) -> float {
                return lastNegativeLatency(events);
            },
            RangeReductionMetadata{
                    .description = "Time of last event with t < 0 (before alignment)",
                    .category = "Event Statistics",
                    .requires_time_series_element = true,
                    .requires_entity_element = true});

    // EventCountInWindow - parameterized
    registry.registerReduction<EventWithId, int, TimeWindowParams>(
            "EventCountInWindow",
            [](std::span<EventWithId const> events, TimeWindowParams const & params) -> int {
                return eventCountInWindow(events, params);
            },
            RangeReductionMetadata{
                    .description = "Count events within a time window",
                    .category = "Event Statistics",
                    .requires_time_series_element = true,
                    .requires_entity_element = true});

    // MeanInterEventInterval - stateless
    registry.registerStatelessReduction<EventWithId, float>(
            "MeanInterEventInterval",
            [](std::span<EventWithId const> events) -> float {
                return meanInterEventInterval(events);
            },
            RangeReductionMetadata{
                    .description = "Mean interval between consecutive events",
                    .category = "Event Statistics",
                    .requires_time_series_element = true,
                    .requires_entity_element = true});

    // EventTimeSpan - stateless
    registry.registerStatelessReduction<EventWithId, float>(
            "EventTimeSpan",
            [](std::span<EventWithId const> events) -> float {
                return eventTimeSpan(events);
            },
            RangeReductionMetadata{
                    .description = "Duration from first to last event",
                    .category = "Event Statistics",
                    .requires_time_series_element = true,
                    .requires_entity_element = true});
}

// ============================================================================
// Value Range Reductions Registration
// ============================================================================

void registerValueRangeReductions() {
    auto & registry = RangeReductionRegistry::instance();

    using TimeValuePoint = AnalogTimeSeries::TimeValuePoint;

    // MaxValue - stateless
    registry.registerStatelessReduction<TimeValuePoint, float>(
            "MaxValue",
            [](std::span<TimeValuePoint const> points) -> float {
                return maxValue(points);
            },
            RangeReductionMetadata{
                    .description = "Maximum value in range",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});

    // MinValue - stateless
    registry.registerStatelessReduction<TimeValuePoint, float>(
            "MinValue",
            [](std::span<TimeValuePoint const> points) -> float {
                return minValue(points);
            },
            RangeReductionMetadata{
                    .description = "Minimum value in range",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});

    // MeanValue - stateless
    registry.registerStatelessReduction<TimeValuePoint, float>(
            "MeanValue",
            [](std::span<TimeValuePoint const> points) -> float {
                return meanValue(points);
            },
            RangeReductionMetadata{
                    .description = "Mean value in range",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});

    // StdValue - stateless
    registry.registerStatelessReduction<TimeValuePoint, float>(
            "StdValue",
            [](std::span<TimeValuePoint const> points) -> float {
                return stdValue(points);
            },
            RangeReductionMetadata{
                    .description = "Standard deviation of values in range",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});

    // TimeOfMax - stateless
    registry.registerStatelessReduction<TimeValuePoint, float>(
            "TimeOfMax",
            [](std::span<TimeValuePoint const> points) -> float {
                return timeOfMax(points);
            },
            RangeReductionMetadata{
                    .description = "Time at which maximum value occurs",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});

    // TimeOfMin - stateless
    registry.registerStatelessReduction<TimeValuePoint, float>(
            "TimeOfMin",
            [](std::span<TimeValuePoint const> points) -> float {
                return timeOfMin(points);
            },
            RangeReductionMetadata{
                    .description = "Time at which minimum value occurs",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});

    // TimeOfThresholdCross - parameterized
    registry.registerReduction<TimeValuePoint, float, ThresholdCrossParams>(
            "TimeOfThresholdCross",
            [](std::span<TimeValuePoint const> points, ThresholdCrossParams const & params) -> float {
                return timeOfThresholdCross(points, params);
            },
            RangeReductionMetadata{
                    .description = "First time value crosses threshold",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});

    // SumValue - stateless
    registry.registerStatelessReduction<TimeValuePoint, float>(
            "SumValue",
            [](std::span<TimeValuePoint const> points) -> float {
                return sumValue(points);
            },
            RangeReductionMetadata{
                    .description = "Sum of all values in range",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});

    // ValueRange - stateless
    registry.registerStatelessReduction<TimeValuePoint, float>(
            "ValueRange",
            [](std::span<TimeValuePoint const> points) -> float {
                return valueRange(points);
            },
            RangeReductionMetadata{
                    .description = "Range of values (max - min)",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});

    // AreaUnderCurve - stateless
    registry.registerStatelessReduction<TimeValuePoint, float>(
            "AreaUnderCurve",
            [](std::span<TimeValuePoint const> points) -> float {
                return areaUnderCurve(points);
            },
            RangeReductionMetadata{
                    .description = "Area under curve (trapezoidal integration)",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true,
                    .is_expensive = true});

    // CountAboveThreshold - parameterized
    registry.registerReduction<TimeValuePoint, int, ThresholdCrossParams>(
            "CountAboveThreshold",
            [](std::span<TimeValuePoint const> points, ThresholdCrossParams const & params) -> int {
                return countAboveThreshold(points, params);
            },
            RangeReductionMetadata{
                    .description = "Count samples above threshold",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});

    // FractionAboveThreshold - parameterized
    registry.registerReduction<TimeValuePoint, float, ThresholdCrossParams>(
            "FractionAboveThreshold",
            [](std::span<TimeValuePoint const> points, ThresholdCrossParams const & params) -> float {
                return fractionAboveThreshold(points, params);
            },
            RangeReductionMetadata{
                    .description = "Fraction of samples above threshold (0.0 to 1.0)",
                    .category = "Value Statistics",
                    .requires_time_series_element = true,
                    .requires_value_element = true});
}

// ============================================================================
// Static Initialization
// ============================================================================

namespace {

// Auto-register all range reductions at static initialization time
[[maybe_unused]] bool const registered = []() {
    registerAllRangeReductions();
    return true;
}();

}// anonymous namespace

}// namespace WhiskerToolbox::Transforms::V2::RangeReductions
