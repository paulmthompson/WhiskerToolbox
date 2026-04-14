/**
 * @file TensorTSNE.hpp
 * @brief TransformsV2 container transform wrapper for t-SNE dimensionality reduction
 *
 * Thin wrapper that calls MLCore::TSNEOperation on TensorData.
 * Preserves the input's RowDescriptor (TimeFrameIndex rows survive reduction).
 */

#ifndef WHISKERTOOLBOX_V2_TENSOR_TSNE_HPP
#define WHISKERTOOLBOX_V2_TENSOR_TSNE_HPP

#include "TransformsV2/utils/NaNFilter.hpp"

#include <cstddef>
#include <memory>

class TensorData;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for t-SNE container transform (reflect-cpp compatible)
 */
struct TensorTSNEParams {
    /// Number of output dimensions (usually 2 or 3)
    std::size_t n_components = 2;
    /// Perplexity (effective number of neighbors, typical range 5–50)
    double perplexity = 30.0;
    /// Barnes-Hut approximation angle (0 = exact, >0 = faster)
    double theta = 0.5;

    /// How to handle rows containing NaN/Inf values
    WhiskerToolbox::Transforms::V2::NaNPolicy nan_policy =
            WhiskerToolbox::Transforms::V2::NaNPolicy::Propagate;
};

/**
 * @brief Apply t-SNE dimensionality reduction to a TensorData
 *
 * Container signature: TensorData → TensorData
 *
 * The input must be 2D. The output has `n_components` columns named
 * "tSNE1", "tSNE2", etc. The input's RowDescriptor is preserved: if the
 * input has TimeFrameIndex rows, the output also has TimeFrameIndex
 * rows with the same time indices.
 *
 * @param input  2D TensorData (observations × features)
 * @param params t-SNE parameters
 * @param ctx    Compute context for progress/cancellation
 * @return Reduced TensorData, or nullptr on failure
 */
[[nodiscard]] auto tensorTSNE(
        TensorData const & input,
        TensorTSNEParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData>;

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_TENSOR_TSNE_HPP
