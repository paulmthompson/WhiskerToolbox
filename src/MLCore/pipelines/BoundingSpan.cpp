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

FilteredPredictions filterPredictionsToIntervals(
        arma::Row<std::size_t> const & predictions,
        std::optional<arma::mat> const & probabilities,
        std::vector<TimeFrameIndex> const & times,
        DigitalIntervalSeries const & intervals) {
    assert(predictions.n_elem == times.size() &&
           "predictions size must match times size");
    assert(!probabilities.has_value() ||
           probabilities->n_cols == times.size() &&
                   "probabilities columns must match times size");

    // Collect all intervals into a sorted vector for efficient lookup
    std::vector<Interval> sorted_intervals;
    sorted_intervals.reserve(intervals.size());
    for (auto const & iwid: intervals.view()) {
        sorted_intervals.push_back(iwid.interval);
    }
    std::sort(sorted_intervals.begin(), sorted_intervals.end());

    // For each time, check if it falls within any interval
    std::vector<arma::uword> keep_indices;
    keep_indices.reserve(times.size());

    for (std::size_t i = 0; i < times.size(); ++i) {
        auto const t = times[i].getValue();
        for (auto const & iv: sorted_intervals) {
            if (t >= iv.start && t <= iv.end) {
                keep_indices.push_back(static_cast<arma::uword>(i));
                break;
            }
            // Since intervals are sorted by start, if start > t we can stop
            if (iv.start > t) {
                break;
            }
        }
    }

    FilteredPredictions result;
    if (keep_indices.empty()) {
        result.predictions.set_size(0);
        if (probabilities.has_value()) {
            result.probabilities = arma::mat(probabilities->n_rows, 0);
        }
        return result;
    }

    arma::uvec col_indices(keep_indices.size());
    for (std::size_t i = 0; i < keep_indices.size(); ++i) {
        col_indices[i] = keep_indices[i];
    }

    result.predictions = predictions.cols(col_indices);
    if (probabilities.has_value()) {
        result.probabilities = probabilities->cols(col_indices);
    }
    result.times.reserve(keep_indices.size());
    for (auto idx: keep_indices) {
        result.times.push_back(times[idx]);
    }
    return result;
}

FilteredTrainingRows filterTrainingRowsToIntervals(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        std::vector<TimeFrameIndex> const & times,
        DigitalIntervalSeries const & intervals) {
    assert(features.n_cols == times.size() && "features columns must match times size");
    assert(labels.n_elem == times.size() && "labels size must match times size");

    // Collect all intervals into a sorted vector for efficient lookup
    std::vector<Interval> sorted_intervals;
    sorted_intervals.reserve(intervals.size());
    for (auto const & iwid: intervals.view()) {
        sorted_intervals.push_back(iwid.interval);
    }
    std::sort(sorted_intervals.begin(), sorted_intervals.end());

    // For each time, check if it falls within any interval
    std::vector<arma::uword> keep_indices;
    keep_indices.reserve(times.size());

    for (std::size_t i = 0; i < times.size(); ++i) {
        auto const t = times[i].getValue();
        for (auto const & iv: sorted_intervals) {
            if (t >= iv.start && t <= iv.end) {
                keep_indices.push_back(static_cast<arma::uword>(i));
                break;
            }
            if (iv.start > t) {
                break;
            }
        }
    }

    if (keep_indices.empty()) {
        return FilteredTrainingRows{
                arma::mat(features.n_rows, 0),
                arma::Row<std::size_t>(),
                {}};
    }

    arma::uvec col_indices(keep_indices.size());
    for (std::size_t i = 0; i < keep_indices.size(); ++i) {
        col_indices[i] = keep_indices[i];
    }

    arma::Row<std::size_t> filtered_labels(keep_indices.size());
    std::vector<TimeFrameIndex> filtered_times;
    filtered_times.reserve(keep_indices.size());
    for (std::size_t i = 0; i < keep_indices.size(); ++i) {
        filtered_labels[i] = labels[keep_indices[i]];
        filtered_times.push_back(times[keep_indices[i]]);
    }

    return FilteredTrainingRows{
            features.cols(col_indices),
            std::move(filtered_labels),
            std::move(filtered_times)};
}

}// namespace MLCore
