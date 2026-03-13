/**
 * @file BatchInferenceResult.hpp
 * @brief Data structures for accumulating off-thread batch inference results.
 *
 * These structs are torch-free and Qt-free so they can be constructed on a
 * worker thread and consumed on the main thread without header conflicts.
 */

#ifndef BATCH_INFERENCE_RESULT_HPP
#define BATCH_INFERENCE_RESULT_HPP

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

#include <string>
#include <variant>
#include <vector>

/// Decoded data produced by a single output decoder for one frame.
using DecodedOutputVariant = std::variant<Mask2D, Point2D<float>, Line2D>;

/// A single decoded result for one frame + one output binding.
struct FrameResult {
    int frame_index = 0;      ///< Frame that was processed
    DecodedOutputVariant data;///< Decoded geometry data
    std::string data_key;     ///< DataManager key to write into
    std::string decoder_id;   ///< Decoder that produced this result
};

/// Accumulated results from an offline batch inference run.
///
/// The worker fills this on its thread; the main thread consumes it to
/// write into DataManager in one bulk pass.
struct BatchInferenceResult {
    std::vector<FrameResult> results;///< Per-frame decoded outputs
    bool success = true;             ///< False if an error occurred
    std::string error_message;       ///< Non-empty when success == false
};

#endif// BATCH_INFERENCE_RESULT_HPP
