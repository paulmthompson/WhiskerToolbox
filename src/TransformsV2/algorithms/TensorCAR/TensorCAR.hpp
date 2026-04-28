/**
 * @file TensorCAR.hpp
 * @brief Common Average Reference (CAR) container transform for multi-channel TensorData.
 *
 * Phase 2.1 of the multi-channel storage roadmap.
 * Takes a 2D TensorData (time × channels) and subtracts a cross-channel
 * reference signal (mean or median across included channels) from every channel.
 */

#ifndef WHISKERTOOLBOX_V2_TENSOR_CAR_HPP
#define WHISKERTOOLBOX_V2_TENSOR_CAR_HPP

#include <memory>
#include <vector>

class TensorData;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Method used to compute the cross-channel reference signal
 */
enum class CARMethod {
    Mean, ///< Arithmetic mean across channels
    Median///< Median across channels (default; more robust to outliers)
};

/**
 * @brief Parameters for Common Average Reference transform (reflect-cpp compatible)
 */
struct TensorCARParams {
    /// Method for computing the cross-channel reference signal
    CARMethod method = CARMethod::Median;

    /// Channel indices (0-based) to exclude from reference computation.
    /// Excluded channels still have the reference subtracted from them.
    std::vector<int> exclude_channels;

    /// When true, request CUDA GPU acceleration (requires TENSOR_BACKEND_LIBTORCH).
    /// When false, use the native backend: LibTorch CPU if data is LibTorch-backed,
    /// Armadillo CPU otherwise. The compute path auto-detects the storage backend
    /// to avoid unnecessary conversions.
    bool use_gpu = false;
};

/**
 * @brief Apply Common Average Reference to a multi-channel TensorData
 *
 * Container signature: TensorData → TensorData
 *
 * For each time point t, computes a reference value r(t) as the mean or median
 * across all non-excluded channels. The reference is then subtracted from every
 * channel at every time point:
 *   result[t, c] = input[t, c] - r(t)
 *
 * The input must be 2D with shape (time × channels). The RowDescriptor
 * (TimeFrameIndex, Interval, or Ordinal) and TimeFrame are preserved intact.
 *
 * @param input  2D TensorData (time × channels)
 * @param params CAR parameters
 * @param ctx    Compute context for progress/cancellation/logging
 * @return CAR-referenced TensorData with the same shape as input, or nullptr on failure
 *
 * @pre input.ndim() == 2
 * @pre input.numRows() >= 1
 * @pre input.numColumns() >= 1
 * @pre exclude_channels does not exclude all channels
 */
[[nodiscard]] auto tensorCAR(
        TensorData const & input,
        TensorCARParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData>;

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_TENSOR_CAR_HPP
