#ifndef WHISKERTOOLBOX_V2_INTERVAL_RANGE_REDUCTIONS_HPP
#define WHISKERTOOLBOX_V2_INTERVAL_RANGE_REDUCTIONS_HPP

/**
 * @file IntervalRangeReductions.hpp
 * @brief Range reductions for interval-based time series (DigitalIntervalSeries)
 *
 * These reductions consume a range of IntervalWithId elements and produce a
 * scalar.  They are designed for trial-aligned analysis where a set of
 * overlapping intervals is gathered per row and reduced to a single value.
 *
 * ## Phase 1.3 — TableView → TensorData Refactoring
 *
 * These reductions replace the following TableView computers:
 *   - IntervalOverlapComputer (CountOverlaps)  → IntervalCount
 *   - IntervalOverlapComputer (AssignID_Start) → IntervalStartExtract
 *   - IntervalOverlapComputer (AssignID_End)   → IntervalEndExtract
 *   - IntervalOverlapComputer (AssignID)       → IntervalSourceIndex
 *
 * @see EventRangeReductions.hpp for event-based reductions
 * @see ValueRangeReductions.hpp for value-based reductions
 * @see RangeReductionRegistry.hpp for registration
 */

#include <cmath>
#include <limits>
#include <span>

namespace WhiskerToolbox::Transforms::V2::RangeReductions {

// ============================================================================
// Interval Reduction Functions
// ============================================================================

/**
 * @brief Count total number of intervals in range
 *
 * Equivalent to the old IntervalOverlapComputer "CountOverlaps" mode.
 *
 * @param intervals Range of IntervalWithId elements
 * @return Number of intervals in the range
 */
template<typename Element>
[[nodiscard]] inline int intervalCount(std::span<Element const> intervals) {
    return static_cast<int>(intervals.size());
}

/**
 * @brief Extract the start time of the first interval in range
 *
 * Equivalent to the old IntervalOverlapComputer "AssignID_Start" mode.
 *
 * @param intervals Range of IntervalWithId elements (should be sorted by start time)
 * @return Start time of first interval as float, or NaN if empty
 */
template<typename Element>
[[nodiscard]] inline float intervalStartExtract(std::span<Element const> intervals) {
    if (intervals.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return static_cast<float>(intervals.front().value().start);
}

/**
 * @brief Extract the end time of the first interval in range
 *
 * Equivalent to the old IntervalOverlapComputer "AssignID_End" mode.
 *
 * @param intervals Range of IntervalWithId elements (should be sorted by start time)
 * @return End time of first interval as float, or NaN if empty
 */
template<typename Element>
[[nodiscard]] inline float intervalEndExtract(std::span<Element const> intervals) {
    if (intervals.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return static_cast<float>(intervals.front().value().end);
}

/**
 * @brief Extract the source index (EntityId) of the first overlapping interval
 *
 * Equivalent to the old IntervalOverlapComputer "AssignID" mode.
 * Returns the EntityId of the first interval as an int, which represents
 * the index of the source interval in the original series.
 *
 * @param intervals Range of IntervalWithId elements
 * @return EntityId of first interval as int, or -1 if empty
 */
template<typename Element>
[[nodiscard]] inline int intervalSourceIndex(std::span<Element const> intervals) {
    if (intervals.empty()) {
        return -1;
    }
    return static_cast<int>(intervals.front().id().id);
}

} // namespace WhiskerToolbox::Transforms::V2::RangeReductions

#endif // WHISKERTOOLBOX_V2_INTERVAL_RANGE_REDUCTIONS_HPP
