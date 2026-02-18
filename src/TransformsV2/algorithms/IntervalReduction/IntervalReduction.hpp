#ifndef WHISKERTOOLBOX_V2_INTERVAL_REDUCTION_HPP
#define WHISKERTOOLBOX_V2_INTERVAL_REDUCTION_HPP

/**
 * @file IntervalReduction.hpp
 * @brief Binary container transforms that produce TensorData from interval + source pairs
 *
 * Phase 3.4 of the TableView → TensorData refactoring plan.
 *
 * These transforms take a DigitalIntervalSeries (defining rows) and a source data
 * container, gather the source data within each interval, apply a named range reduction,
 * and produce a TensorData with interval-based RowDescriptor and a single column.
 *
 * Three variants are provided for different source types:
 * - AnalogIntervalReduction: (DigitalIntervalSeries, AnalogTimeSeries) → TensorData
 * - EventIntervalReduction: (DigitalIntervalSeries, DigitalEventSeries) → TensorData
 * - IntervalOverlapReduction: (DigitalIntervalSeries, DigitalIntervalSeries) → TensorData
 *
 * Example JSON pipeline usage:
 * @code
 * {
 *   "step_id": "reduce_analog",
 *   "transform_name": "AnalogIntervalReduction",
 *   "input_key": "trial_intervals",
 *   "additional_input_keys": ["firing_rate"],
 *   "output_key": "trial_mean_rate",
 *   "parameters": {
 *     "reduction_name": "MeanValue",
 *     "column_name": "mean_firing_rate"
 *   }
 * }
 * @endcode
 *
 * @see GatherResult for the underlying gather pattern
 * @see RangeReductionRegistry for available reductions
 * @see TensorData::createFromIntervals for output construction
 */

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <string>

class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class TensorData;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Parameters for interval reduction transforms
 *
 * Controls which range reduction is applied to data gathered within each interval.
 *
 * Example JSON:
 * ```json
 * {
 *   "reduction_name": "MeanValue",
 *   "column_name": "mean_firing_rate"
 * }
 * ```
 */
struct IntervalReductionParams {
    /**
     * @brief Name of the range reduction to apply
     *
     * Must be a reduction registered in RangeReductionRegistry.
     * The reduction must accept the element type of the source data:
     * - AnalogTimeSeries → TimeValuePoint reductions (MeanValue, MaxValue, etc.)
     * - DigitalEventSeries → EventWithId reductions (EventCount, EventPresence, etc.)
     * - DigitalIntervalSeries → IntervalWithId reductions (IntervalCount, etc.)
     *
     * Default: "MeanValue"
     */
    std::optional<std::string> reduction_name;

    /**
     * @brief JSON string for reduction-specific parameters
     *
     * Most built-in reductions are stateless and don't need parameters.
     * Pass "{}" or omit for stateless reductions.
     *
     * Default: "{}"
     */
    std::optional<std::string> reduction_params_json;

    /**
     * @brief Name of the output column in the produced TensorData
     *
     * Default: "value"
     */
    std::optional<std::string> column_name;

    // Helper methods with defaults
    [[nodiscard]] std::string getReductionName() const {
        return reduction_name.value_or("MeanValue");
    }

    [[nodiscard]] std::string getReductionParamsJson() const {
        return reduction_params_json.value_or("{}");
    }

    [[nodiscard]] std::string getColumnName() const {
        return column_name.value_or("value");
    }
};

// ============================================================================
// Transform Implementations (Binary Container Transforms)
// ============================================================================

/**
 * @brief Gather analog data within intervals and reduce to a TensorData column
 *
 * This is a **binary container transform** because:
 * - Requires the full DigitalIntervalSeries to define row structure
 * - Requires the full AnalogTimeSeries for data access within each interval
 * - Handles cross-TimeFrame conversion between interval and analog coordinate systems
 * - Produces a TensorData with interval-based RowDescriptor
 *
 * For each interval, the transform:
 * 1. Gathers AnalogTimeSeries samples within the interval range
 * 2. Applies the named range reduction (e.g., MeanValue, MaxValue)
 * 3. Produces one float value per interval
 *
 * The output TensorData has:
 * - Interval-based RowDescriptor (one row per interval)
 * - A single column named by params.column_name
 * - TimeFrame inherited from the interval series
 *
 * @param intervals DigitalIntervalSeries defining the row structure
 * @param analog AnalogTimeSeries to gather and reduce
 * @param params Parameters controlling the reduction
 * @param ctx Compute context for progress reporting and cancellation
 * @return TensorData with one column of reduced values
 */
std::shared_ptr<TensorData> analogIntervalReduction(
        DigitalIntervalSeries const & intervals,
        AnalogTimeSeries const & analog,
        IntervalReductionParams const & params,
        ComputeContext const & ctx);

/**
 * @brief Gather event data within intervals and reduce to a TensorData column
 *
 * Same pattern as analogIntervalReduction but for DigitalEventSeries.
 * Common reductions: EventCount, EventPresence, FirstPositiveLatency, etc.
 *
 * @param intervals DigitalIntervalSeries defining the row structure
 * @param events DigitalEventSeries to gather and reduce
 * @param params Parameters controlling the reduction
 * @param ctx Compute context for progress reporting and cancellation
 * @return TensorData with one column of reduced values
 */
std::shared_ptr<TensorData> eventIntervalReduction(
        DigitalIntervalSeries const & intervals,
        DigitalEventSeries const & events,
        IntervalReductionParams const & params,
        ComputeContext const & ctx);

/**
 * @brief Gather overlapping intervals and reduce to a TensorData column
 *
 * Same pattern as analogIntervalReduction but for DigitalIntervalSeries.
 * Common reductions: IntervalCount, IntervalStartExtract, IntervalSourceIndex, etc.
 *
 * @param intervals DigitalIntervalSeries defining the row structure
 * @param source DigitalIntervalSeries to gather and reduce (e.g., stimulus intervals)
 * @param params Parameters controlling the reduction
 * @param ctx Compute context for progress reporting and cancellation
 * @return TensorData with one column of reduced values
 */
std::shared_ptr<TensorData> intervalOverlapReduction(
        DigitalIntervalSeries const & intervals,
        DigitalIntervalSeries const & source,
        IntervalReductionParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_INTERVAL_REDUCTION_HPP
