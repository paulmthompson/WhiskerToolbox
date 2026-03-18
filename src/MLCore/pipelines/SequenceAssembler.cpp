/**
 * @file SequenceAssembler.cpp
 * @brief Implementation of contiguous temporal sequence segmentation
 */

#include "SequenceAssembler.hpp"

#include <cassert>

namespace MLCore {

namespace {

/**
 * @brief Find the column indices where a new sequence begins (gap boundaries)
 *
 * A boundary at index i means times[i] - times[i-1] > 1 (non-consecutive).
 * Index 0 is always implicitly a boundary (start of the first sequence).
 *
 * @param times Sorted TimeFrameIndex values
 * @return Boundary indices where new sequences start (always includes 0)
 */
[[nodiscard]] std::vector<std::size_t> findBoundaries(
        std::vector<TimeFrameIndex> const & times) {

    std::vector<std::size_t> boundaries;
    boundaries.push_back(0);

    for (std::size_t i = 1; i < times.size(); ++i) {
        auto const diff = times[i] - times[i - 1];
        if (diff.getValue() > 1) {
            boundaries.push_back(i);
        }
    }

    return boundaries;
}

/**
 * @brief Extract a sub-segment from the flat matrix/labels/times
 *
 * @param features Full feature matrix (features × observations)
 * @param labels Full label vector (may be empty for unlabeled segmentation)
 * @param times Full time vector
 * @param start Start column index (inclusive)
 * @param end End column index (exclusive)
 * @return SequenceSegment for the [start, end) range
 */
[[nodiscard]] SequenceSegment extractSegment(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        std::vector<TimeFrameIndex> const & times,
        std::size_t start,
        std::size_t end) {

    auto const length = end - start;

    SequenceSegment seg;
    seg.features = features.cols(static_cast<arma::uword>(start),
                                 static_cast<arma::uword>(end - 1));

    if (labels.n_elem > 0) {
        seg.labels = labels.subvec(static_cast<arma::uword>(start),
                                   static_cast<arma::uword>(end - 1));
    }

    seg.times.reserve(length);
    for (std::size_t i = start; i < end; ++i) {
        seg.times.push_back(times[i]);
    }

    return seg;
}

}// namespace

// ============================================================================
// Labeled segmentation
// ============================================================================

std::vector<SequenceSegment> SequenceAssembler::segment(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        std::vector<TimeFrameIndex> const & times,
        SequenceAssemblerConfig const & config) {

    assert(features.n_cols == times.size() &&
           "SequenceAssembler: features columns must match times size");
    assert((labels.n_elem == 0 || labels.n_elem == times.size()) &&
           "SequenceAssembler: labels length must match times size or be empty");

    if (times.empty()) {
        return {};
    }

    auto const boundaries = findBoundaries(times);

    std::vector<SequenceSegment> segments;
    segments.reserve(boundaries.size());

    for (std::size_t b = 0; b < boundaries.size(); ++b) {
        auto const start = boundaries[b];
        auto const end = (b + 1 < boundaries.size())
                                 ? boundaries[b + 1]
                                 : times.size();
        auto const length = end - start;

        if (length >= config.min_sequence_length) {
            segments.push_back(extractSegment(features, labels, times, start, end));
        }
    }

    return segments;
}

// ============================================================================
// Unlabeled segmentation
// ============================================================================

std::vector<SequenceSegment> SequenceAssembler::segment(
        arma::mat const & features,
        std::vector<TimeFrameIndex> const & times,
        SequenceAssemblerConfig const & config) {

    assert(features.n_cols == times.size() &&
           "SequenceAssembler: features columns must match times size");

    // Delegate to the labeled overload with an empty label vector
    arma::Row<std::size_t> const empty_labels;
    return segment(features, empty_labels, times, config);
}

}// namespace MLCore
