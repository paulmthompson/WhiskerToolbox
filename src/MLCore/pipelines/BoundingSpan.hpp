/**
 * @file BoundingSpan.hpp
 * @brief Bounding span computation for prediction region filtering
 *
 * Provides utilities to compute the temporal bounding span across
 * training label intervals, prediction intervals, and feature tensor
 * row times. Used by the classification pipeline to determine which
 * rows to include when predicting on a subset of frames (so sequence
 * models retain full temporal context across the bounding range).
 */

#ifndef MLCORE_BOUNDINGSPAN_HPP
#define MLCORE_BOUNDINGSPAN_HPP

#include "TimeFrame/TimeFrameIndex.hpp"

#include <armadillo>
#include <cstdint>
#include <optional>
#include <vector>

class DigitalIntervalSeries;

namespace MLCore {

/**
 * @brief A closed temporal range [min_time, max_time]
 */
struct BoundingSpan {
    TimeFrameIndex min_time;
    TimeFrameIndex max_time;
};

/**
 * @brief Compute the bounding span of all intervals in a series
 *
 * @pre intervals must not be empty
 * @return [min(start), max(end)] across all intervals, or nullopt if empty
 */
[[nodiscard]] std::optional<BoundingSpan> computeIntervalBounds(
        DigitalIntervalSeries const & intervals);

/**
 * @brief Compute the bounding span of a vector of time indices
 *
 * @pre row_times must not be empty
 * @return [min, max] of all times, or nullopt if empty
 */
[[nodiscard]] std::optional<BoundingSpan> computeRowTimeBounds(
        std::vector<TimeFrameIndex> const & row_times);

/**
 * @brief Merge two bounding spans into their union (the span covering both)
 */
[[nodiscard]] BoundingSpan mergeSpans(BoundingSpan a, BoundingSpan b);

/**
 * @brief Result of filtering features and times to a bounding span
 */
struct FilteredRows {
    arma::mat features;
    std::vector<TimeFrameIndex> times;
};

/**
 * @brief Filter feature columns and times to only those within a bounding span
 *
 * Keeps only columns (observations) whose corresponding time falls within
 * [span.min_time, span.max_time] (inclusive).
 *
 * @pre features.n_cols == times.size()
 * @param features  Feature matrix (rows = features, cols = observations)
 * @param times     Time index for each observation column
 * @param span      Bounding span to filter to
 * @return Filtered features and times (may be empty if no rows fall in span)
 */
[[nodiscard]] FilteredRows filterRowsToSpan(
        arma::mat const & features,
        std::vector<TimeFrameIndex> const & times,
        BoundingSpan span);

}// namespace MLCore

#endif// MLCORE_BOUNDINGSPAN_HPP
