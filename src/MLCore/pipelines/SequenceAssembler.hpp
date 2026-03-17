#ifndef MLCORE_SEQUENCEASSEMBLER_HPP
#define MLCORE_SEQUENCEASSEMBLER_HPP

/**
 * @file SequenceAssembler.hpp
 * @brief Segments flat feature matrices into contiguous temporal sequences
 *
 * Sequence models like HMMs require observations grouped into contiguous
 * temporal sequences. TensorData may have gaps in its TimeFrameIndex rows
 * (e.g., frames 100–200, then 500–600). The SequenceAssembler detects
 * these gaps and splits the flat feature matrix into separate segments.
 *
 * A gap is defined as time[i+1] - time[i] > 1 (i.e., non-consecutive
 * TimeFrameIndex values). Each contiguous run is emitted as a separate
 * SequenceSegment containing the feature sub-matrix, optional labels,
 * and the original time indices.
 */

#include "TimeFrame/TimeFrameIndex.hpp"

#include <armadillo>

#include <cstddef>
#include <vector>

namespace MLCore {

// ============================================================================
// Result types
// ============================================================================

/**
 * @brief A single contiguous temporal segment of features and labels
 *
 * Represents a run of consecutive TimeFrameIndex values with no gaps.
 * The feature matrix follows mlpack convention: features × segment_length.
 */
struct SequenceSegment {
    /// Feature sub-matrix (features × segment_length), mlpack column-major layout
    arma::mat features;

    /// Label row vector (1 × segment_length). Empty if segmented without labels.
    arma::Row<std::size_t> labels;

    /// Original TimeFrameIndex for each column, preserving temporal provenance
    std::vector<TimeFrameIndex> times;
};

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Configuration for sequence segmentation
 */
struct SequenceAssemblerConfig {
    /// Discard sequences shorter than this length (default: 2).
    /// Single-frame sequences carry no temporal information for HMMs.
    std::size_t min_sequence_length = 2;
};

// ============================================================================
// Assembler API
// ============================================================================

/**
 * @brief Segments flat matrices into contiguous temporal sequences
 *
 * @pre times must be sorted in ascending order
 * @pre features.n_cols == times.size()
 * @pre labels.n_elem == times.size() (for the labeled overload)
 */
class SequenceAssembler {
public:
    /**
     * @brief Segment features + labels + times into contiguous sequences
     *
     * @param features Feature matrix (features × observations), mlpack layout
     * @param labels Label row vector (1 × observations)
     * @param times TimeFrameIndex for each observation column, must be sorted
     * @param config Segmentation parameters
     * @return Vector of SequenceSegment, one per contiguous run meeting the
     *         minimum length requirement. May be empty if all runs are too short.
     *
     * @pre features.n_cols == times.size()
     * @pre labels.n_elem == times.size()
     * @pre times is sorted in ascending order
     */
    [[nodiscard]] static std::vector<SequenceSegment> segment(
            arma::mat const & features,
            arma::Row<std::size_t> const & labels,
            std::vector<TimeFrameIndex> const & times,
            SequenceAssemblerConfig const & config = {});

    /**
     * @brief Segment features + times into contiguous sequences (no labels)
     *
     * Used for prediction-only passes where labels are not available.
     *
     * @param features Feature matrix (features × observations), mlpack layout
     * @param times TimeFrameIndex for each observation column, must be sorted
     * @param config Segmentation parameters
     * @return Vector of SequenceSegment with empty label vectors.
     *
     * @pre features.n_cols == times.size()
     * @pre times is sorted in ascending order
     */
    [[nodiscard]] static std::vector<SequenceSegment> segment(
            arma::mat const & features,
            std::vector<TimeFrameIndex> const & times,
            SequenceAssemblerConfig const & config = {});
};

}// namespace MLCore

#endif// MLCORE_SEQUENCEASSEMBLER_HPP
