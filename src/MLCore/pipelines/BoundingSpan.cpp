/**
 * @file BoundingSpan.cpp
 * @brief Implementation of bounding span computation utilities
 */

#include "BoundingSpan.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

namespace MLCore {

std::optional<BoundingSpan> computeIntervalBounds(
        DigitalIntervalSeries const & intervals) {
    if (intervals.size() == 0) {
        return std::nullopt;
    }

    auto min_val = std::numeric_limits<int64_t>::max();
    auto max_val = std::numeric_limits<int64_t>::min();

    for (auto const & iwid: intervals.view()) {
        min_val = std::min(min_val, iwid.interval.start);
        max_val = std::max(max_val, iwid.interval.end);
    }

    return BoundingSpan{TimeFrameIndex(min_val), TimeFrameIndex(max_val)};
}

std::optional<BoundingSpan> computeRowTimeBounds(
        std::vector<TimeFrameIndex> const & row_times) {
    if (row_times.empty()) {
        return std::nullopt;
    }

    auto const [min_it, max_it] = std::minmax_element(row_times.begin(), row_times.end());
    return BoundingSpan{*min_it, *max_it};
}

BoundingSpan mergeSpans(BoundingSpan a, BoundingSpan b) {
    return BoundingSpan{
            std::min(a.min_time, b.min_time),
            std::max(a.max_time, b.max_time)};
}

FilteredRows filterRowsToSpan(
        arma::mat const & features,
        std::vector<TimeFrameIndex> const & times,
        BoundingSpan span) {
    assert(features.n_cols == times.size() && "features columns must match times size");

    // Collect indices of columns within the span
    std::vector<arma::uword> keep_indices;
    keep_indices.reserve(times.size());

    std::vector<TimeFrameIndex> kept_times;
    kept_times.reserve(times.size());

    for (std::size_t i = 0; i < times.size(); ++i) {
        if (times[i] >= span.min_time && times[i] <= span.max_time) {
            keep_indices.push_back(static_cast<arma::uword>(i));
            kept_times.push_back(times[i]);
        }
    }

    if (keep_indices.empty()) {
        return FilteredRows{arma::mat(features.n_rows, 0), {}};
    }

    arma::uvec col_indices(keep_indices.size());
    for (std::size_t i = 0; i < keep_indices.size(); ++i) {
        col_indices[i] = keep_indices[i];
    }

    return FilteredRows{features.cols(col_indices), std::move(kept_times)};
}

}// namespace MLCore
