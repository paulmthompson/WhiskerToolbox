#ifndef NEURALYZER_V2_NORMALIZE_TIME_HPP
#define NEURALYZER_V2_NORMALIZE_TIME_HPP

/**
 * @file NormalizeTime.hpp
 * @brief Temporal normalization transforms for trial-aligned analysis
 *
 * This file provides transforms that normalize event and analog time series
 * relative to a reference point (alignment time). This is essential for
 * trial-aligned analysis such as raster plots where each trial's events
 * need to be centered around a common reference (t=0).
 *
 * ## Primary Use Case: Raster Plots
 *
 * ```
 * Trial 0: [100, 200] alignment=100
 *   Event at 125 → normalized to +25
 *   Event at 175 → normalized to +75
 *
 * Trial 1: [300, 450] alignment=300
 *   Event at 285 → normalized to -15  (before trial start)
 *   Event at 350 → normalized to +50
 * ```
 *
 * Use NormalizeTimeParamsV2 with parameter bindings from PipelineValueStore:
 *
 * ```cpp
 * // Pipeline JSON with bindings
 * {
 *   "steps": [{
 *     "transform": "NormalizeTimeValueV2",
 *     "param_bindings": {"alignment_time": "alignment_time"}
 *   }]
 * }
 *
 * // At runtime
 * auto store = buildGatherRowStore(gather_result, trial_idx);  // Populates alignment_time
 * // Pipeline applies bindings automatically
 * ```
 *
 * ## Output Types
 *
 * Clock-tick inputs normalize to `ClockTicks` offsets. Index-space inputs
 * (`TimeFrameIndex`, `EventWithId`, analog samples) still return `float` for
 * sub-frame precision where applicable.
 *
 * @see GatherResult for trial-aligned data gathering
 * @see PipelineValueStore for value store documentation
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/ClockTicksReflector.hpp"
#include "core/ComputeContext.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace Neuralyzer::Transforms::V2 {

// ============================================================================
// V1 Parameters (manual alignment time setting)
// ============================================================================


// ============================================================================
// V2 Parameters (using param bindings)
// ============================================================================

/**
 * @brief Parameters for time normalization using value store bindings (V2 pattern)
 *
 * This is the recommended V2 replacement for NormalizeTimeParams that uses regular fields
 * populated via JSON parameter bindings instead of manual setAlignmentTime().
 *
 * ## Key Differences from NormalizeTimeParams
 *
 * - `alignment_time` is a regular ClockTicks field, not rfl::Skip
 * - No setAlignmentTime() method - values come from pipeline bindings
 * - Fully serializable via reflect-cpp
 *
 * ## Usage with Pipeline Bindings
 *
 * @code
 * // JSON pipeline definition
 * {
 *   "steps": [{
 *     "transform": "NormalizeTimeValueV2",
 *     "params": {},
 *     "param_bindings": {"alignment_time": "alignment_time"}
 *   }]
 * }
 *
 * // At runtime with GatherResult
 * auto store = buildGatherRowStore(gather_result, i);  // Contains "alignment_time"
 * // Pipeline applies bindings automatically
 * @endcode
 *
 * @see NormalizeTimeParams for legacy approach
 * @see PipelineValueStore for value store documentation
 * @see ParameterBinding.hpp for binding mechanism
 */
struct NormalizeTimeParamsV2 {
    /// Alignment time (t=0 reference point) - populated via binding
    ClockTicks alignment_time{0};
};

/**
 * @brief Parameters for shifting all DigitalEventSeries event times.
 */
struct ShiftDigitalEventSeriesParams {
    /// Clock-tick offset added to every event time.
    int64_t offset{0};
};

// ============================================================================
// V2 Transform Functions
// ============================================================================

/**
 * @brief Normalize a TimeFrameIndex to float value (V2 - uses bound params)
 *
 * This is the V2 version that uses NormalizeTimeParamsV2 with regular fields
 * populated via parameter bindings.
 *
 * @param time Input time to normalize
 * @param params Parameters with alignment_time bound from store
 * @return float The normalized time (time - alignment_time)
 *
 * @code
 * TimeFrameIndex event_time{125};
 * NormalizeTimeParamsV2 params{.alignment_time = 100};
 *
 * float norm_time = normalizeTimeValueV2(event_time, params);
 * // norm_time == 25.0f
 * @endcode
 */
[[nodiscard]] inline float normalizeTimeValueV2(
        TimeFrameIndex const & time,
        NormalizeTimeParamsV2 const & params) {
    return static_cast<float>(time.getValue() - params.alignment_time.getValue());
}

/**
 * @brief Normalize a ClockTicks value (V2 - uses bound params)
 *
 * @param time Input clock-tick time to normalize
 * @param params Parameters with alignment_time bound from store
 * @return ClockTicks The normalized time (time - alignment_time)
 */
[[nodiscard]] inline ClockTicks normalizeClockTicksValueV2(
        ClockTicks const & time,
        NormalizeTimeParamsV2 const & params) {
    return ClockTicks{time.getValue() - params.alignment_time.getValue()};
}

/**
 * @brief Normalize event time to float value (V2 - uses bound params)
 *
 * Convenience function for EventWithId that extracts time and normalizes.
 *
 * @param event Input event with absolute time
 * @param params Parameters with alignment_time bound from store
 * @return float The normalized time (event.time() - alignment_time)
 */
[[nodiscard]] inline float normalizeEventTimeValueV2(
        EventWithId const & event,
        NormalizeTimeParamsV2 const & params) {
    return static_cast<float>(event.time().getValue() - params.alignment_time.getValue());
}

// NOTE: A ClockTicksWithId-specific normalize transform is intentionally not
// registered. TransformPipeline value projections operate on ElementVariant
// types, and ElementVariant contains ClockTicks but not ClockTicksWithId. When
// bindValueProjectionV2 receives a ClockTicksWithId view element it extracts
// element.time() and passes ClockTicks into the pipeline. The original
// ClockTicksWithId remains available to the caller for EntityId access outside
// the TransformPipeline. Therefore NormalizeClockTicksValueV2 is the canonical
// pipeline transform for DigitalEventSeries view elements.
//
// If ElementVariant is later extended to include ClockTicksWithId, this helper
// can be restored and registered deliberately.
// [[nodiscard]] inline float normalizeClockTicksWithIdValueV2(
//         ClockTicksWithId const & event,
//         NormalizeTimeParamsV2 const & params) {
//     return static_cast<float>(event.time() - params.alignment_time);
// }

/**
 * @brief Normalize analog sample time to float value (V2 - uses bound params)
 *
 * @param sample Input sample with absolute time
 * @param params Parameters with alignment_time bound from store
 * @return float The normalized time (sample.time() - alignment_time)
 */
[[nodiscard]] inline float normalizeSampleTimeValueV2(
        AnalogTimeSeries::TimeValuePoint const & sample,
        NormalizeTimeParamsV2 const & params) {
    return static_cast<float>(sample.time().getValue() - params.alignment_time.getValue());
}

/**
 * @brief Normalize every event in a DigitalEventSeries into relative ClockTicks.
 *
 * Container signature: DigitalEventSeries -> DigitalEventSeries.
 * The output is backed by relative ClockTicks storage and preserves event EntityIds.
 *
 * @param input Input event series whose view() exposes ClockTicksWithId values
 * @param params Parameters containing the alignment time to subtract
 * @param ctx Compute context for cancellation
 * @return Relative-time DigitalEventSeries with no TimeFrame
 */
[[nodiscard]] inline std::shared_ptr<DigitalEventSeries> normalizeDigitalEventSeriesRelative(
        DigitalEventSeries const & input,
        NormalizeTimeParamsV2 const & params,
        ComputeContext const & ctx) {
    std::vector<ClockTicks> events;
    std::vector<EntityId> entity_ids;
    events.reserve(input.size());
    entity_ids.reserve(input.size());

    for (auto const & event: input.view()) {
        if (ctx.shouldCancel()) {
            break;
        }
        events.emplace_back(event.time().getValue() - params.alignment_time.getValue());
        entity_ids.push_back(event.id());
    }

    return DigitalEventSeries::createFromRelativeClockTicks(std::move(events), std::move(entity_ids));
}

/**
 * @brief Shift every event in a DigitalEventSeries by a fixed clock-tick offset.
 *
 * Container signature: DigitalEventSeries -> DigitalEventSeries.
 * The output preserves absolute storage and TimeFrame for absolute-time series,
 * and uses relative storage for relative-time input series.
 */
[[nodiscard]] inline std::shared_ptr<DigitalEventSeries> shiftDigitalEventSeries(
        DigitalEventSeries const & input,
        ShiftDigitalEventSeriesParams const & params,
        ComputeContext const & ctx) {
    std::vector<ClockTicks> shifted_events;
    std::vector<EntityId> entity_ids;
    shifted_events.reserve(input.size());
    entity_ids.reserve(input.size());

    for (auto const & event: input.view()) {
        if (ctx.shouldCancel()) {
            break;
        }
        shifted_events.emplace_back(event.time().getValue() + params.offset);
        entity_ids.push_back(event.id());
    }

    if (input.storesRelativeTimes()) {
        return DigitalEventSeries::createFromRelativeClockTicks(
                std::move(shifted_events), std::move(entity_ids));
    }

    std::vector<TimeFrameIndex> shifted_indices;
    shifted_indices.reserve(shifted_events.size());
    for (auto const & shifted_event: shifted_events) {
        shifted_indices.emplace_back(shifted_event.getValue());
    }

    auto result = std::make_shared<DigitalEventSeries>(std::move(shifted_indices));
    if (input.getTimeFrame()) {
        result->setTimeFrame(input.getTimeFrame());
    }
    return result;
}

}// namespace Neuralyzer::Transforms::V2

#endif// NEURALYZER_V2_NORMALIZE_TIME_HPP
