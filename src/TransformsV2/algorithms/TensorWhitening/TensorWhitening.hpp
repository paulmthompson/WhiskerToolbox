/**
 * @file TensorWhitening.hpp
 * @brief Robust ZCA whitening container transform for multi-channel TensorData.
 */

#ifndef WHISKERTOOLBOX_V2_TENSOR_WHITENING_HPP
#define WHISKERTOOLBOX_V2_TENSOR_WHITENING_HPP

#include <memory>
#include <vector>

class TensorData;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for robust ZCA whitening of multi-channel tensors.
 */
struct TensorWhiteningParams {
    /// MAD threshold multiplier used to reject high-amplitude samples when
    /// estimating the covariance. Set <= 0 to disable robust sample exclusion.
    float mad_threshold_multiplier = 4.0F;

    /// Channel indices (0-based) to exclude from whitening.
    /// Excluded channels are copied through unchanged.
    std::vector<int> exclude_channels;

    /// Rescale whitened channels by the mean robust channel amplitude.
    /// Enabled by default to preserve a familiar output scale.
    bool rescale_amplitude = true;

    /// Diagonal regularization added to the covariance eigenvalues.
    float epsilon = 1.0e-8F;

    /// When true, request CUDA execution. Falls back to LibTorch CPU when CUDA
    /// is unavailable.
    bool use_gpu = false;
};

/**
 * @brief Apply robust ZCA whitening to the included channels of a 2D tensor.
 *
 * The input is interpreted as time x channels. Included channels are median-
 * centered, optionally filtered by a MAD-based clean-sample mask for covariance
 * estimation, whitened with a ZCA matrix, and optionally rescaled in amplitude.
 * Excluded channels are passed through unchanged.
 *
 * The transform always uses the LibTorch compute/storage path so the result is
 * LibTorch-backed on both CPU and GPU builds.
 *
 * @param input 2D TensorData with shape (time, channel)
 * @param params Whitening parameters
 * @param ctx Compute context for progress, logging, and cancellation
 * @return Whitened TensorData on success, or nullptr on validation/backend failure
 *
 * @pre input.ndim() == 2
 * @pre input.numRows() >= 2
 * @pre input.numColumns() >= 1
 * @pre epsilon > 0
 * @pre exclude_channels does not exclude all channels
 */
[[nodiscard]] auto tensorWhitening(
        TensorData const & input,
        TensorWhiteningParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData>;

}// namespace WhiskerToolbox::Transforms::V2::Examples


#endif// WHISKERTOOLBOX_V2_TENSOR_WHITENING_HPP