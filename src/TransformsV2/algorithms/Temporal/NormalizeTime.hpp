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
 * auto store = gather_result.buildTrialStore(trial_idx);  // Populates alignment_time
 * // Pipeline applies bindings automatically
 * ```
 *
 * ## Output Types
 *
 * The transforms return float values representing normalized time offsets.
 * Using float allows sub-frame precision and negative values
 * (events before alignment point).
 *
 * @see GatherResult for trial-aligned data gathering
 * @see PipelineValueStore for value store documentation
 */

#include "DigitalTimeSeries/EventWithId.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>
#include <stdexcept>

namespace WhiskerToolbox::Transforms::V2 {

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
 * - `alignment_time` is a regular int64_t field, not rfl::Skip
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
 * auto store = gather_result.buildTrialStore(i);  // Contains "alignment_time"
 * // Pipeline applies bindings automatically
 * @endcode
 *
 * @see NormalizeTimeParams for legacy approach
 * @see PipelineValueStore for value store documentation
 * @see ParameterBinding.hpp for binding mechanism
 */
struct NormalizeTimeParamsV2 {
    /// Alignment time (t=0 reference point) - populated via binding
    int64_t alignment_time = 0;
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
        TimeFrameIndex const& time,
        NormalizeTimeParamsV2 const& params) {
    return static_cast<float>(time.getValue() - params.alignment_time);
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
        EventWithId const& event,
        NormalizeTimeParamsV2 const& params) {
    return static_cast<float>(event.time().getValue() - params.alignment_time);
}

/**
 * @brief Normalize analog sample time to float value (V2 - uses bound params)
 *
 * @param sample Input sample with absolute time
 * @param params Parameters with alignment_time bound from store
 * @return float The normalized time (sample.time() - alignment_time)
 */
[[nodiscard]] inline float normalizeSampleTimeValueV2(
        AnalogTimeSeries::TimeValuePoint const& sample,
        NormalizeTimeParamsV2 const& params) {
    return static_cast<float>(sample.time().getValue() - params.alignment_time);
}

}  // namespace WhiskerToolbox::Transforms::V2

#endif  // NEURALYZER_V2_NORMALIZE_TIME_HPP
