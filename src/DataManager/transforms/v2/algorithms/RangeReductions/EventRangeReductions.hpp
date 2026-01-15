#ifndef WHISKERTOOLBOX_V2_EVENT_RANGE_REDUCTIONS_HPP
#define WHISKERTOOLBOX_V2_EVENT_RANGE_REDUCTIONS_HPP

/**
 * @file EventRangeReductions.hpp
 * @brief Range reductions for event-based time series (DigitalEventSeries)
 *
 * These reductions consume a range of event elements and produce a scalar.
 * They are designed for trial-aligned analysis where each trial view needs
 * to be reduced to a single value (e.g., for sorting, partitioning, coloring).
 *
 * ## Distinction from TimeGroupedTransform
 *
 * Range reductions collapse an ENTIRE range to a scalar:
 *   `range<EventWithId>` → `float` or `int`
 *
 * TimeGroupedTransform operates on elements at ONE time point:
 *   `span<Element>` at time T → `vector<Element>` at time T
 *
 * ## Usage with GatherResult
 *
 * ```cpp
 * auto spike_gather = gather(spikes, trials);
 *
 * // Sort trials by first-spike latency
 * auto latencies = spike_gather.reducePipeline<EventWithId, float>(
 *     TransformPipeline()
 *         .addStep("NormalizeEventTime", NormalizeTimeParams{})
 *         .addRangeReduction("FirstPositiveLatency", NoReductionParams{}));
 * ```
 *
 * @see RangeReductionTypes.hpp for concepts
 * @see RangeReductionRegistry.hpp for registration
 */

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <cmath>
#include <limits>
#include <span>

namespace WhiskerToolbox::Transforms::V2::RangeReductions {

// ============================================================================
// Parameter Types
// ============================================================================

/**
 * @brief Parameters for time window filtering
 *
 * Used by EventCountInWindow to count events within a specific time range.
 * Times are relative to whatever normalization has been applied to the input.
 */
struct TimeWindowParams {
    /// Start of time window (inclusive)
    float window_start{0.0f};

    /// End of time window (exclusive)
    float window_end{std::numeric_limits<float>::max()};
};

// ============================================================================
// Reduction Functions
// ============================================================================

/**
 * @brief Count total number of events in range
 *
 * This is a stateless reduction that simply returns the count of elements.
 *
 * @tparam Element Event element type (must have time() accessor)
 * @param events Range of events
 * @return Number of events in the range
 *
 * @example
 * ```cpp
 * auto events = trial_view.view();
 * int spike_count = eventCount(std::span{events.begin(), events.end()});
 * ```
 */
template<typename Element>
[[nodiscard]] inline int eventCount(std::span<Element const> events) {
    return static_cast<int>(events.size());
}

/**
 * @brief Find time of first event with positive time
 *
 * Returns the time of the first event where time() > 0.
 * This is useful for computing first-spike latency after trial alignment.
 *
 * @tparam Element Event element type (must have time() accessor returning numeric type)
 * @param events Range of events (should be sorted by time)
 * @return Time of first positive-time event, or NaN if none found
 *
 * @note Input should be sorted by time for meaningful results.
 *       After NormalizeTime transform, t=0 is the alignment point.
 */
template<typename Element>
[[nodiscard]] inline float firstPositiveLatency(std::span<Element const> events) {
    for (auto const & e : events) {
        // Convert to float for comparison - handles TimeFrameIndex and float
        float time_val = static_cast<float>(e.time().getValue());
        if (time_val > 0.0f) {
            return time_val;
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

/**
 * @brief Find time of last event with negative time
 *
 * Returns the time of the last event where time() < 0.
 * This is useful for analyzing pre-stimulus activity.
 *
 * @tparam Element Event element type (must have time() accessor returning numeric type)
 * @param events Range of events (should be sorted by time)
 * @return Time of last negative-time event, or NaN if none found
 *
 * @note Input should be sorted by time for meaningful results.
 *       After NormalizeTime transform, t=0 is the alignment point.
 */
template<typename Element>
[[nodiscard]] inline float lastNegativeLatency(std::span<Element const> events) {
    float last_negative = std::numeric_limits<float>::quiet_NaN();
    for (auto const & e : events) {
        float time_val = static_cast<float>(e.time().getValue());
        if (time_val < 0.0f) {
            last_negative = time_val;
        } else {
            // Since input should be sorted, we can stop once we hit non-negative
            // But we continue to handle unsorted input gracefully
        }
    }
    return last_negative;
}

/**
 * @brief Count events within a time window
 *
 * Counts events where window_start <= time() < window_end.
 * This is parameterized to allow runtime-configurable windows.
 *
 * @tparam Element Event element type (must have time() accessor returning numeric type)
 * @param events Range of events
 * @param params Time window parameters
 * @return Number of events in the window
 *
 * @example
 * ```cpp
 * // Count spikes in first 100ms after stimulus
 * TimeWindowParams params{.window_start = 0.0f, .window_end = 100.0f};
 * int early_spikes = eventCountInWindow(events, params);
 * ```
 */
template<typename Element>
[[nodiscard]] inline int eventCountInWindow(
        std::span<Element const> events,
        TimeWindowParams const & params) {
    int count = 0;
    for (auto const & e : events) {
        float time_val = static_cast<float>(e.time().getValue());
        if (time_val >= params.window_start && time_val < params.window_end) {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Compute inter-event interval statistics (mean)
 *
 * Calculates the mean interval between consecutive events.
 * Useful for characterizing firing rate or rhythmicity.
 *
 * @tparam Element Event element type (must have time() accessor returning numeric type)
 * @param events Range of events (should be sorted by time)
 * @return Mean inter-event interval, or NaN if fewer than 2 events
 */
template<typename Element>
[[nodiscard]] inline float meanInterEventInterval(std::span<Element const> events) {
    if (events.size() < 2) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    float total_interval = 0.0f;
    for (size_t i = 1; i < events.size(); ++i) {
        float t1 = static_cast<float>(events[i - 1].time().getValue());
        float t2 = static_cast<float>(events[i].time().getValue());
        total_interval += (t2 - t1);
    }

    return total_interval / static_cast<float>(events.size() - 1);
}

/**
 * @brief Get the time span of events (last - first)
 *
 * Returns the duration from first to last event in the range.
 *
 * @tparam Element Event element type (must have time() accessor returning numeric type)
 * @param events Range of events (should be sorted by time)
 * @return Time span (last.time() - first.time()), or 0 if fewer than 2 events
 */
template<typename Element>
[[nodiscard]] inline float eventTimeSpan(std::span<Element const> events) {
    if (events.size() < 2) {
        return 0.0f;
    }

    float first_time = static_cast<float>(events.front().time().getValue());
    float last_time = static_cast<float>(events.back().time().getValue());
    return last_time - first_time;
}

}// namespace WhiskerToolbox::Transforms::V2::RangeReductions

#endif// WHISKERTOOLBOX_V2_EVENT_RANGE_REDUCTIONS_HPP
