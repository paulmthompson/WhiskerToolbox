/**
 * @file TensorICA.cpp
 * @brief Implementation of ICA (RADICAL) container transform wrapper
 */

#include "TensorICA.hpp"

#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/unsupervised/ICAOperation.hpp"
#include "Tensors/TensorData.hpp"
#include "TransformsV2/utils/DimReductionOutputBuilder.hpp"
#include "TransformsV2/utils/NaNFilter.hpp"
#include "core/ComputeContext.hpp"

#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

auto tensorICA(
        TensorData const & input,
        TensorICAParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData> {

    // Validate input is 2D
    if (input.ndim() != 2) {
        ctx.logMessage("TensorICA: Input must be 2D, got " +
                       std::to_string(input.ndim()) + "D");
        return nullptr;
    }

    std::size_t const num_rows = input.numRows();
    std::size_t const num_cols = input.numColumns();

    if (num_rows < 2) {
        ctx.logMessage("TensorICA: Need at least 2 observations, got " +
                       std::to_string(num_rows));
        return nullptr;
    }

    if (num_cols < 2) {
        ctx.logMessage("TensorICA: Need at least 2 features, got " +
                       std::to_string(num_cols));
        return nullptr;
    }

    ctx.reportProgress(0);

    // Convert TensorData (row-major: rows=observations, cols=features)
    // to observations × features double matrix for NaN filtering
    arma::fmat const & fmat = input.asArmadilloMatrix();

    // NaN handling: check and filter based on policy
    auto const nan_policy = params.nan_policy;
    bool const has_nan = hasNonFiniteRows(fmat);

    if (has_nan && nan_policy == NaNPolicy::Fail) {
        ctx.logMessage("TensorICA: Input contains NaN/Inf and nan_policy is Fail");
        return nullptr;
    }

    // Convert to double observation matrix (obs × features)
    arma::mat obs_matrix = arma::conv_to<arma::mat>::from(fmat);

    // Filter NaN rows if needed
    NaNFilterResult filter_result;
    if (has_nan) {
        filter_result = filterNonFiniteRows(obs_matrix);
        ctx.logMessage("TensorICA: Filtered " + std::to_string(filter_result.rows_dropped) +
                       " NaN/Inf rows (" + std::to_string(filter_result.valid_row_indices.size()) +
                       " remaining)");

        if (filter_result.valid_row_indices.size() < 2) {
            ctx.logMessage("TensorICA: Too few finite rows after filtering");
            return nullptr;
        }
    } else {
        // All rows finite — build full index vector
        filter_result.clean_matrix = std::move(obs_matrix);
        filter_result.valid_row_indices.resize(num_rows);
        for (std::size_t i = 0; i < num_rows; ++i) {
            filter_result.valid_row_indices[i] = i;
        }
        filter_result.rows_dropped = 0;
    }

    // Transpose to mlpack convention (features × observations)
    arma::mat const features = filter_result.clean_matrix.t();

    if (ctx.shouldCancel()) {
        return nullptr;
    }
    ctx.reportProgress(5);

    // Run ICA via MLCore
    MLCore::ICAOperation ica;
    MLCore::ICAParameters mlparams;
    mlparams.noise_std_dev = params.noise_std_dev;
    mlparams.replicates = params.replicates;
    mlparams.angles = params.angles;
    mlparams.sweeps = params.sweeps;

    arma::mat result;
    if (!ica.fitTransform(features, &mlparams, result)) {
        ctx.logMessage("TensorICA: ICA (RADICAL) computation failed");
        return nullptr;
    }

    if (ctx.shouldCancel()) {
        return nullptr;
    }
    ctx.reportProgress(90);

    // ICA output has same number of dimensions as input
    std::size_t const n_components = result.n_rows;

    // Generate column names: IC1, IC2, ...
    std::vector<std::string> col_names;
    col_names.reserve(n_components);
    for (std::size_t i = 0; i < n_components; ++i) {
        col_names.push_back("IC" + std::to_string(i + 1));
    }

    // Delegate output construction to shared builder
    auto output = buildDimReductionOutput(
            result, input, filter_result.valid_row_indices,
            nan_policy, n_components, col_names);

    ctx.reportProgress(100);
    return output;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
