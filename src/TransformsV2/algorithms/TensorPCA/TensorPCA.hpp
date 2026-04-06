/**
 * @file TensorPCA.hpp
 * @brief TransformsV2 container transform wrapper for PCA dimensionality reduction
 *
 * Thin wrapper that calls MLCore::PCAOperation on TensorData.
 * Preserves the input's RowDescriptor (TimeFrameIndex rows survive reduction).
 */

#ifndef WHISKERTOOLBOX_V2_TENSOR_PCA_HPP
#define WHISKERTOOLBOX_V2_TENSOR_PCA_HPP

#include <cstddef>
#include <memory>
#include <optional>

class TensorData;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for PCA container transform (reflect-cpp compatible)
 */
struct TensorPCAParams {
    /// Number of principal components to retain
    std::optional<std::size_t> n_components;

    [[nodiscard]] std::size_t getNComponents() const { return n_components.value_or(2); }
};

/**
 * @brief Apply PCA dimensionality reduction to a TensorData
 *
 * Container signature: TensorData → TensorData
 *
 * The input must be 2D. The output has `n_components` columns named
 * "PC1", "PC2", etc. The input's RowDescriptor is preserved: if the
 * input has TimeFrameIndex rows, the output also has TimeFrameIndex
 * rows with the same time indices.
 *
 * @param input  2D TensorData (observations × features)
 * @param params PCA parameters
 * @param ctx    Compute context for progress/cancellation
 * @return Reduced TensorData, or nullptr on failure
 */
[[nodiscard]] auto tensorPCA(
        TensorData const & input,
        TensorPCAParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData>;

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_TENSOR_PCA_HPP
