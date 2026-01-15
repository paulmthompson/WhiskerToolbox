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
 * The transforms produce new element types with normalized time values:
 *
 * - `NormalizedEvent`: Like EventWithId but with float normalized_time
 * - `NormalizedValue`: Like TimeValuePoint but with float normalized_time
 *
 * Using float for normalized time allows sub-frame precision and negative values
 * (events before alignment point).
 *
 * @see ContextAwareParams.hpp for context injection infrastructure
 * @see GatherResult for trial-aligned data gathering
 * @see EventRangeReductions.hpp for reductions on normalized events
 */

#include "DigitalTimeSeries/EventWithId.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/v2/core/ContextAwareParams.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>
#include <stdexcept>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Normalized Element Types
// ============================================================================

/**
 * @brief Event with normalized time relative to alignment point
 *
 * Represents an event whose time has been shifted relative to a reference point.
 * The original entity information is preserved.
 *
 * ## Time Representation
 *
 * - `normalized_time`: Float offset from alignment (can be negative)
 * - `original_time`: Original TimeFrameIndex (for reference)
 *
 * ## Concept Satisfaction
 *
 * Satisfies TimeSeriesElement via time() (returns normalized_time as float-based index)
 * Satisfies EntityElement via id()
 * Satisfies ValueElement via value() (returns normalized_time)
 *
 * Note: time() returns the normalized_time cast to TimeFrameIndex, which may
 * truncate sub-frame precision. Use normalized_time directly when precision matters.
 */
struct NormalizedEvent {
    /// Time offset from alignment point (can be negative, sub-frame precision)
    float normalized_time{0.0f};

    /// Original absolute time (for reference/debugging)
    TimeFrameIndex original_time{0};

    /// Entity ID from original event
    EntityId entity_id{0};

    NormalizedEvent() = default;

    NormalizedEvent(float norm_time, TimeFrameIndex orig_time, EntityId id)
        : normalized_time(norm_time),
          original_time(orig_time),
          entity_id(id) {}

    // ========== Concept Accessors ==========

    /**
     * @brief Get normalized time as TimeFrameIndex (for TimeSeriesElement concept)
     * @return TimeFrameIndex The normalized time (truncated to integer)
     * @note Use normalized_time field directly for full float precision
     */
    [[nodiscard]] constexpr TimeFrameIndex time() const noexcept {
        return TimeFrameIndex{static_cast<int64_t>(normalized_time)};
    }

    /**
     * @brief Get entity ID (for EntityElement concept)
     */
    [[nodiscard]] constexpr EntityId id() const noexcept { return entity_id; }

    /**
     * @brief Get normalized time as value (for ValueElement concept)
     */
    [[nodiscard]] constexpr float value() const noexcept { return normalized_time; }

    // ========== Comparison ==========

    [[nodiscard]] bool operator==(NormalizedEvent const& other) const noexcept {
        return normalized_time == other.normalized_time &&
               original_time == other.original_time &&
               entity_id == other.entity_id;
    }
};

/**
 * @brief Analog value with normalized time relative to alignment point
 *
 * Represents a time-value sample whose time has been shifted relative to
 * a reference point. The original value is preserved.
 *
 * ## Time Representation
 *
 * - `normalized_time`: Float offset from alignment (can be negative)
 * - `sample_value`: Original analog value (unchanged)
 *
 * ## Concept Satisfaction
 *
 * Satisfies TimeSeriesElement via time()
 * Satisfies ValueElement via value() (returns sample_value, NOT normalized_time)
 *
 * Note: Unlike NormalizedEvent, value() returns the sample_value, not the time.
 * This preserves the original semantic of "value" for analog data.
 */
struct NormalizedValue {
    /// Time offset from alignment point (can be negative, sub-frame precision)
    float normalized_time{0.0f};

    /// Original sample value (unchanged)
    float sample_value{0.0f};

    /// Original absolute time (for reference/debugging)
    TimeFrameIndex original_time{0};

    NormalizedValue() = default;

    NormalizedValue(float norm_time, float value, TimeFrameIndex orig_time)
        : normalized_time(norm_time),
          sample_value(value),
          original_time(orig_time) {}

    // ========== Concept Accessors ==========

    /**
     * @brief Get normalized time as TimeFrameIndex (for TimeSeriesElement concept)
     */
    [[nodiscard]] constexpr TimeFrameIndex time() const noexcept {
        return TimeFrameIndex{static_cast<int64_t>(normalized_time)};
    }

    /**
     * @brief Get sample value (for ValueElement concept)
     * @note This returns the analog value, NOT the normalized time
     */
    [[nodiscard]] constexpr float value() const noexcept { return sample_value; }

    // ========== Comparison ==========

    [[nodiscard]] bool operator==(NormalizedValue const& other) const noexcept {
        return normalized_time == other.normalized_time &&
               sample_value == other.sample_value &&
               original_time == other.original_time;
    }
};

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

/**
 * @brief Normalize event time relative to alignment point
 *
 * Computes: normalized_time = event.time() - alignment_time
 *
 * @param event Input event with absolute time
 * @param params Parameters containing alignment time from context
 * @return NormalizedEvent with time offset from alignment
 * @throws std::runtime_error if params.alignment_time is not set
 *
 * Example:
 * ```cpp
 * EventWithId event{TimeFrameIndex{125}, EntityId{1}};
 * NormalizeTimeParams params;
 * params.setAlignmentTime(TimeFrameIndex{100});
 *
 * auto normalized = normalizeEventTime(event, params);
 * // normalized.normalized_time == 25.0f
 * // normalized.original_time == 125
 * // normalized.entity_id == 1
 * ```
 */
[[nodiscard]] inline NormalizedEvent normalizeEventTime(
        EventWithId const& event,
        NormalizeTimeParams const& params) {
    TimeFrameIndex alignment = params.getAlignmentTime();
    float offset = static_cast<float>(event.time().getValue() - alignment.getValue());

    return NormalizedEvent{
        offset,
        event.time(),
        event.id()
    };
}

/**
 * @brief Normalize analog sample time relative to alignment point
 *
 * Computes: normalized_time = sample.time() - alignment_time
 * The sample value is preserved unchanged.
 *
 * @param sample Input sample with absolute time
 * @param params Parameters containing alignment time from context
 * @return NormalizedValue with time offset from alignment and original value
 * @throws std::runtime_error if params.alignment_time is not set
 *
 * Example:
 * ```cpp
 * TimeValuePoint sample{TimeFrameIndex{150}, 3.5f};
 * NormalizeTimeParams params;
 * params.setAlignmentTime(TimeFrameIndex{100});
 *
 * auto normalized = normalizeValueTime(sample, params);
 * // normalized.normalized_time == 50.0f
 * // normalized.sample_value == 3.5f
 * // normalized.original_time == 150
 * ```
 */
[[nodiscard]] inline NormalizedValue normalizeValueTime(
        AnalogTimeSeries::TimeValuePoint const& sample,
        NormalizeTimeParams const& params) {
    TimeFrameIndex alignment = params.getAlignmentTime();
    float offset = static_cast<float>(sample.time().getValue() - alignment.getValue());

    return NormalizedValue{
        offset,
        sample.value(),
        sample.time()
    };
}

// ============================================================================
// Value Projection Functions (Return float directly)
// ============================================================================

/**
 * @brief Normalize event time to float value (value projection)
 *
 * This is the **value projection** version of normalizeEventTime.
 * Instead of returning a NormalizedEvent struct, it returns just the
 * normalized time as a float. The EntityId can be obtained from the
 * source EventWithId directly.
 *
 * ## Use Case: Trial-Aligned Analysis
 *
 * When drawing raster plots or computing statistics, you often need:
 * - The normalized time (computed via projection)
 * - The EntityId (from the source element)
 *
 * Using value projection avoids creating intermediate types and keeps
 * the transform pipeline simpler.
 *
 * @param event Input event with absolute time
 * @param params Parameters containing alignment time from context
 * @return float The normalized time (event.time() - alignment_time)
 * @throws std::runtime_error if params.alignment_time is not set
 *
 * Example:
 * ```cpp
 * EventWithId event{TimeFrameIndex{125}, EntityId{1}};
 * NormalizeTimeParams params;
 * params.setAlignmentTime(TimeFrameIndex{100});
 *
 * float norm_time = normalizeEventTimeValue(event, params);
 * // norm_time == 25.0f
 * // event.id() still available from source
 * ```
 *
 * @see ValueProjectionTypes.hpp for value projection infrastructure
 * @see makeValueView() for lazy iteration with projections
 */
[[nodiscard]] inline float normalizeEventTimeValue(
        EventWithId const& event,
        NormalizeTimeParams const& params) {
    TimeFrameIndex alignment = params.getAlignmentTime();
    return static_cast<float>(event.time().getValue() - alignment.getValue());
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

}  // namespace WhiskerToolbox::Transforms::V2

#endif  // WHISKERTOOLBOX_V2_NORMALIZE_TIME_HPP
