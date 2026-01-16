#ifndef WHISKERTOOLBOX_V2_NORMALIZE_TIME_HPP
#define WHISKERTOOLBOX_V2_NORMALIZE_TIME_HPP

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
 * ## Context-Aware Design
 *
 * NormalizeTimeParams is context-aware, receiving alignment time from TrialContext:
 *
 * ```cpp
 * NormalizeTimeParams params;
 * TrialContext ctx{.alignment_time = TimeFrameIndex{100}};
 * params.setContext(ctx);  // Captures alignment time
 *
 * // Transform now uses cached alignment
 * auto normalized = normalizeEventTime(event, params);  // event.time() - 100
 * ```
 *
 * ## Output Types
 *
 * The transforms return float values representing normalized time offsets.
 * Using float allows sub-frame precision and negative values
 * (events before alignment point).
 *
 * @see ContextAwareParams.hpp for context injection infrastructure
 * @see GatherResult for trial-aligned data gathering
 */

#include "DigitalTimeSeries/EventWithId.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/v2/extension/ContextAwareParams.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>
#include <stdexcept>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Parameters
// ============================================================================

/**
 * @brief Parameters for time normalization transforms
 *
 * This parameter struct is context-aware and receives alignment time from
 * TrialContext. The alignment time is used to compute normalized times
 * for each element.
 *
 * ## Context Injection
 *
 * ```cpp
 * NormalizeTimeParams params;
 * TrialContext ctx{.alignment_time = TimeFrameIndex{100}};
 * params.setContext(ctx);
 *
 * // Now params.getAlignmentTime() returns 100
 * ```
 *
 * ## Manual Override
 *
 * For testing or non-trial use cases, alignment can be set directly:
 *
 * ```cpp
 * NormalizeTimeParams params;
 * params.setAlignmentTime(TimeFrameIndex{50});
 * ```
 *
 * ## Serialization
 *
 * The alignment_time is marked with rfl::Skip because it comes from context
 * at runtime, not from JSON configuration.
 */
struct NormalizeTimeParams {
    // ========== Runtime Context (NOT Serialized) ==========

    /**
     * @brief Cached alignment time from context
     *
     * Set by setContext() or setAlignmentTime().
     * rfl::Skip prevents serialization to JSON.
     */
    rfl::Skip<std::optional<TimeFrameIndex>> alignment_time;

    // ========== Context Injection Interface ==========

    /**
     * @brief Receive context from pipeline
     * @param ctx Trial context containing alignment information
     */
    void setContext(TrialContext const& ctx) {
        alignment_time.value() = ctx.alignment_time;
    }

    /**
     * @brief Check if context has been set
     * @return true if alignment time is available
     */
    [[nodiscard]] bool hasContext() const noexcept {
        return alignment_time.value().has_value();
    }

    // ========== Manual Configuration ==========

    /**
     * @brief Set alignment time directly (for testing or manual use)
     * @param time The alignment time to use
     */
    void setAlignmentTime(TimeFrameIndex time) {
        alignment_time.value() = time;
    }

    /**
     * @brief Get the alignment time
     * @return The alignment time
     * @throws std::runtime_error if context has not been set
     */
    [[nodiscard]] TimeFrameIndex getAlignmentTime() const {
        if (!alignment_time.value().has_value()) {
            throw std::runtime_error("NormalizeTimeParams: alignment time not set. "
                                    "Call setContext() or setAlignmentTime() first.");
        }
        return *alignment_time.value();
    }
};

// ============================================================================
// Transform Functions
// ============================================================================

// ============================================================================
// Value Projection Functions (Return float directly)
// ============================================================================

/**
 * @brief Normalize a TimeFrameIndex to float value (value projection)
 *
 * Computes the offset from an alignment time as a float.
 * This is the fundamental temporal normalization transform.
 *
 * ## Use Case: Trial-Aligned Analysis
 *
 * When drawing raster plots or computing statistics, you typically:
 * 1. Extract `.time()` from an EventWithId or similar element
 * 2. Pass the TimeFrameIndex through this transform
 * 3. Use the float result for plotting/analysis
 * 4. Access EntityId directly from the source element
 *
 * This separation keeps transforms simple and focused.
 *
 * @param time Input time to normalize
 * @param params Parameters containing alignment time from context
 * @return float The normalized time (time - alignment_time)
 * @throws std::runtime_error if params.alignment_time is not set
 *
 * Example:
 * ```cpp
 * TimeFrameIndex event_time{125};
 * NormalizeTimeParams params;
 * params.setAlignmentTime(TimeFrameIndex{100});
 *
 * float norm_time = normalizeTimeValue(event_time, params);
 * // norm_time == 25.0f
 * ```
 *
 * @see ValueProjectionTypes.hpp for value projection infrastructure
 */
[[nodiscard]] inline float normalizeTimeValue(
        TimeFrameIndex const& time,
        NormalizeTimeParams const& params) {
    TimeFrameIndex alignment = params.getAlignmentTime();
    return static_cast<float>(time.getValue() - alignment.getValue());
}

/**
 * @brief Normalize analog sample time to float value (value projection)
 *
 * This is the **value projection** version of normalizeValueTime.
 * Returns only the normalized time, not the sample value.
 *
 * @param sample Input sample with absolute time
 * @param params Parameters containing alignment time from context
 * @return float The normalized time (sample.time() - alignment_time)
 * @throws std::runtime_error if params.alignment_time is not set
 *
 * Example:
 * ```cpp
 * TimeValuePoint sample{TimeFrameIndex{150}, 3.5f};
 * NormalizeTimeParams params;
 * params.setAlignmentTime(TimeFrameIndex{100});
 *
 * float norm_time = normalizeSampleTimeValue(sample, params);
 * // norm_time == 50.0f
 * // sample.value() still available from source
 * ```
 */
[[nodiscard]] inline float normalizeSampleTimeValue(
        AnalogTimeSeries::TimeValuePoint const& sample,
        NormalizeTimeParams const& params) {
    TimeFrameIndex alignment = params.getAlignmentTime();
    return static_cast<float>(sample.time().getValue() - alignment.getValue());
}

// ============================================================================
// V2 Parameters (using param bindings instead of context injection)
// ============================================================================

/**
 * @brief Parameters for time normalization using value store bindings (V2 pattern)
 *
 * This is the V2 replacement for NormalizeTimeParams that uses regular fields
 * populated via JSON parameter bindings instead of context injection.
 *
 * ## Key Differences from NormalizeTimeParams
 *
 * - `alignment_time` is a regular int64_t field, not rfl::Skip
 * - No setContext() method - values come from pipeline bindings
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
 * // At runtime
 * PipelineValueStore store;
 * store.set("alignment_time", trial_start);  // From buildTrialStore()
 *
 * // Bindings automatically populate params.alignment_time
 * @endcode
 *
 * @see NormalizeTimeParams for legacy context-injection approach
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

#endif  // WHISKERTOOLBOX_V2_NORMALIZE_TIME_HPP
