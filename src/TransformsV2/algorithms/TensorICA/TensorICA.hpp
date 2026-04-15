/**
 * @file TensorICA.hpp
 * @brief TransformsV2 container transform wrapper for ICA (RADICAL algorithm)
 *
 * Thin wrapper that calls MLCore::ICAOperation on TensorData.
 * Preserves the input's RowDescriptor (TimeFrameIndex rows survive reduction).
 */

#ifndef WHISKERTOOLBOX_V2_TENSOR_ICA_HPP
#define WHISKERTOOLBOX_V2_TENSOR_ICA_HPP

#include "TransformsV2/utils/NaNFilter.hpp"

#include <cstddef>
#include <memory>

class TensorData;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for ICA container transform (reflect-cpp compatible)
 */
struct TensorICAParams {
    /// Standard deviation of Gaussian noise added to data
    double noise_std_dev = 0.175;

    /// Number of Gaussian-perturbed replicates per point
    std::size_t replicates = 30;

    /// Number of angles in brute-force search during 2D RADICAL
    std::size_t angles = 150;

    /// Number of sweeps (0 = number of dimensions minus one)
    std::size_t sweeps = 0;

    /// How to handle rows containing NaN/Inf values
    WhiskerToolbox::Transforms::V2::NaNPolicy nan_policy =
            WhiskerToolbox::Transforms::V2::NaNPolicy::Propagate;
};

/**
 * @brief Apply ICA (RADICAL) to a TensorData
 *
 * Container signature: TensorData → TensorData
 *
 * The input must be 2D. The output has the same number of columns as the
 * input, with columns named "IC1", "IC2", etc. The input's RowDescriptor
 * is preserved: if the input has TimeFrameIndex rows, the output also has
 * TimeFrameIndex rows with the same time indices.
 *
 * @note ICA does not reduce dimensionality — the output has the same number
 *       of features as the input. Each output column is an independent component.
 * @note RADICAL scales quadratically in the number of dimensions. Consider
 *       reducing dimensionality with PCA first for high-dimensional data.
 *
 * @param input  2D TensorData (observations × features)
 * @param params ICA parameters
 * @param ctx    Compute context for progress/cancellation
 * @return Transformed TensorData, or nullptr on failure
 */
[[nodiscard]] auto tensorICA(
        TensorData const & input,
        TensorICAParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData>;

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_TENSOR_ICA_HPP
