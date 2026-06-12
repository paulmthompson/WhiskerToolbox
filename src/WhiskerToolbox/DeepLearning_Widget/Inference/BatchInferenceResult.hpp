/**
 * @file BatchInferenceResult.hpp
 * @brief Data structures for accumulating off-thread batch inference results.
 *
 * These structs are torch-free and Qt-free so they can be constructed on a
 * worker thread and consumed on the main thread without header conflicts.
 */

#ifndef BATCH_INFERENCE_RESULT_HPP
#define BATCH_INFERENCE_RESULT_HPP

#include "channel_decoding/DecoderDispatch.hpp"

#include <string>
#include <vector>

/// Alias for geometry decoded by channel decoders (shared with DecoderDispatch).
using DecodedOutputVariant = dl::DecodedGeometryVariant;

/**
 * @brief A single decoded result for one frame + one output binding.
 */
struct FrameResult {
    int frame_index = 0;       ///< Frame that was processed
    DecodedOutputVariant data; ///< Decoded geometry data or feature vector
    std::string data_key;      ///< DataManager key to write into
};

/**
 * @brief Accumulated results from an offline batch inference run.
 *
 * The worker fills this on its thread; the main thread consumes it to
 * write into DataManager in one bulk pass.
 */
struct BatchInferenceResult {
    std::vector<FrameResult> results;///< Per-frame decoded outputs
    bool success = true;             ///< False if an error occurred
    std::string error_message;       ///< Non-empty when success == false
};

#endif// BATCH_INFERENCE_RESULT_HPP
