/**
 * @file TensorRobustPCA.hpp
 * @brief TransformsV2 container transform wrapper for Robust PCA dimensionality reduction
 *
 * Thin wrapper that calls MLCore::RobustPCAOperation on TensorData.
 * Preserves the input's RowDescriptor (TimeFrameIndex rows survive reduction).
 */

#ifndef WHISKERTOOLBOX_V2_TENSOR_ROBUST_PCA_HPP
#define WHISKERTOOLBOX_V2_TENSOR_ROBUST_PCA_HPP

#include <cstddef>
#include <memory>
#include <optional>

class TensorData;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for Robust PCA container transform (reflect-cpp compatible)
 */
struct TensorRobustPCAParams {
    /// Number of output dimensions
    std::optional<std::size_t> n_components;
    /// Regularization parameter (0 = auto-compute from data)
    std::optional<double> lambda;
    /// Convergence tolerance
    std::optional<double> tol;
    /// Maximum ALM iterations
    std::optional<std::size_t> max_iter;

    [[nodiscard]] std::size_t getNComponents() const { return n_components.value_or(2); }
    [[nodiscard]] double getLambda() const { return lambda.value_or(0.0); }
    [[nodiscard]] double getTol() const { return tol.value_or(1e-5); }
    [[nodiscard]] std::size_t getMaxIter() const { return max_iter.value_or(100); }
};

/**
 * @brief Apply Robust PCA dimensionality reduction to a TensorData
 *
 * Container signature: TensorData -> TensorData
 *
 * The input must be 2D. The output has `n_components` columns named
 * "RPCA1", "RPCA2", etc. The input's RowDescriptor is preserved: if the
 * input has TimeFrameIndex rows, the output also has TimeFrameIndex
 * rows with the same time indices.
 *
 * @param input  2D TensorData (observations x features)
 * @param params Robust PCA parameters
 * @param ctx    Compute context for progress/cancellation
 * @return Reduced TensorData, or nullptr on failure
 */
[[nodiscard]] auto tensorRobustPCA(
        TensorData const & input,
        TensorRobustPCAParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData>;

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_TENSOR_ROBUST_PCA_HPP
