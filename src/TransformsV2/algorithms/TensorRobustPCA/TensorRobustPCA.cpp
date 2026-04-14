/**
 * @file TensorRobustPCA.cpp
 * @brief Implementation of Robust PCA container transform wrapper
 */

#include "TensorRobustPCA.hpp"

#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/unsupervised/RobustPCAOperation.hpp"
#include "Tensors/TensorData.hpp"
#include "TransformsV2/utils/DimReductionOutputBuilder.hpp"
#include "TransformsV2/utils/NaNFilter.hpp"
#include "core/ComputeContext.hpp"

#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

auto tensorRobustPCA(
        TensorData const & input,
        TensorRobustPCAParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData> {

    // Validate input is 2D
    if (input.ndim() != 2) {
        ctx.logMessage("TensorRobustPCA: Input must be 2D, got " +
                       std::to_string(input.ndim()) + "D");
        return nullptr;
    }

    std::size_t const num_rows = input.numRows();
    std::size_t const num_cols = input.numColumns();

    if (num_rows < 2) {
        ctx.logMessage("TensorRobustPCA: Need at least 2 observations, got " +
                       std::to_string(num_rows));
        return nullptr;
    }

    std::size_t const n_components = params.n_components;
    if (n_components == 0 || n_components > num_cols) {
        ctx.logMessage("TensorRobustPCA: n_components (" + std::to_string(n_components) +
                       ") must be in [1, " + std::to_string(num_cols) + "]");
        return nullptr;
    }

    ctx.reportProgress(0);

    // Convert TensorData to observations × features double matrix
    arma::fmat const & fmat = input.asArmadilloMatrix();

    // NaN handling: check and filter based on policy
    auto const nan_policy = params.nan_policy;
    bool const has_nan = hasNonFiniteRows(fmat);

    if (has_nan && nan_policy == NaNPolicy::Fail) {
        ctx.logMessage("TensorRobustPCA: Input contains NaN/Inf and nan_policy is Fail");
        return nullptr;
    }

    arma::mat obs_matrix = arma::conv_to<arma::mat>::from(fmat);

    NaNFilterResult filter_result;
    if (has_nan) {
        filter_result = filterNonFiniteRows(obs_matrix);
        ctx.logMessage("TensorRobustPCA: Filtered " + std::to_string(filter_result.rows_dropped) +
                       " NaN/Inf rows (" + std::to_string(filter_result.valid_row_indices.size()) +
                       " remaining)");

        if (filter_result.valid_row_indices.size() < 2) {
            ctx.logMessage("TensorRobustPCA: Too few finite rows after filtering");
            return nullptr;
        }
    } else {
        filter_result.clean_matrix = std::move(obs_matrix);
        filter_result.valid_row_indices.resize(num_rows);
        for (std::size_t i = 0; i < num_rows; ++i) {
            filter_result.valid_row_indices[i] = i;
        }
        filter_result.rows_dropped = 0;
    }

    // Transpose to mlpack convention (features x observations)
    arma::mat const features = filter_result.clean_matrix.t();

    if (ctx.shouldCancel()) {
        return nullptr;
    }
    ctx.reportProgress(5);

    // Run Robust PCA via MLCore
    MLCore::RobustPCAOperation rpca;
    MLCore::RobustPCAParameters mlparams;
    mlparams.n_components = n_components;
    mlparams.lambda = params.lambda;
    mlparams.tol = params.tol;
    mlparams.max_iter = params.max_iter;

    arma::mat result;
    if (!rpca.fitTransform(features, &mlparams, result)) {
        ctx.logMessage("TensorRobustPCA: Robust PCA computation failed");
        return nullptr;
    }

    if (ctx.shouldCancel()) {
        return nullptr;
    }
    ctx.reportProgress(90);

    auto const actual_components = static_cast<std::size_t>(result.n_rows);

    // Generate column names: RPCA1, RPCA2, ...
    std::vector<std::string> col_names;
    col_names.reserve(actual_components);
    for (std::size_t i = 0; i < actual_components; ++i) {
        col_names.push_back("RPCA" + std::to_string(i + 1));
    }

    // Delegate output construction to shared builder
    auto output = buildDimReductionOutput(
            result, input, filter_result.valid_row_indices,
            nan_policy, actual_components, col_names);

    ctx.reportProgress(100);
    return output;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
