/**
 * @file TensorPCA.cpp
 * @brief Implementation of PCA container transform wrapper
 */

#include "TensorPCA.hpp"

#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/unsupervised/PCAOperation.hpp"
#include "Tensors/TensorData.hpp"
#include "core/ComputeContext.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

auto tensorPCA(
        TensorData const & input,
        TensorPCAParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData> {

    // Validate input is 2D
    if (input.ndim() != 2) {
        ctx.logMessage("TensorPCA: Input must be 2D, got " +
                       std::to_string(input.ndim()) + "D");
        return nullptr;
    }

    std::size_t const num_rows = input.numRows();
    std::size_t const num_cols = input.numColumns();

    if (num_rows < 2) {
        ctx.logMessage("TensorPCA: Need at least 2 observations, got " +
                       std::to_string(num_rows));
        return nullptr;
    }

    std::size_t const n_components = params.getNComponents();
    if (n_components == 0 || n_components > num_cols) {
        ctx.logMessage("TensorPCA: n_components (" + std::to_string(n_components) +
                       ") must be in [1, " + std::to_string(num_cols) + "]");
        return nullptr;
    }

    ctx.reportProgress(0);

    // Convert TensorData (row-major: rows=observations, cols=features)
    // to mlpack convention (features × observations)
    arma::fmat const & fmat = input.asArmadilloMatrix();
    arma::mat const features = arma::conv_to<arma::mat>::from(fmat.t());

    if (ctx.shouldCancel()) {
        return nullptr;
    }
    ctx.reportProgress(10);

    // Run PCA via MLCore
    MLCore::PCAOperation pca;
    MLCore::PCAParameters mlparams;
    mlparams.n_components = n_components;
    mlparams.scale = params.getScale();

    arma::mat result;
    if (!pca.fitTransform(features, &mlparams, result)) {
        ctx.logMessage("TensorPCA: PCA computation failed");
        return nullptr;
    }

    if (ctx.shouldCancel()) {
        return nullptr;
    }
    ctx.reportProgress(80);

    // Convert back: result is (n_components × observations) → (observations × n_components)
    arma::fmat output_fmat = arma::conv_to<arma::fmat>::from(result.t());

    // Generate column names: PC1, PC2, ...
    std::vector<std::string> col_names;
    col_names.reserve(n_components);
    for (std::size_t i = 0; i < n_components; ++i) {
        col_names.push_back("PC" + std::to_string(i + 1));
    }

    // Armadillo stores column-major, but TensorData factories expect row-major flat data.
    // Extract elements in row-major order.
    std::vector<float> flat_data(output_fmat.n_elem);
    for (arma::uword r = 0; r < output_fmat.n_rows; ++r) {
        for (arma::uword c = 0; c < output_fmat.n_cols; ++c) {
            flat_data[r * n_components + c] = output_fmat(r, c);
        }
    }

    // Build output preserving RowDescriptor from input
    auto const & row_desc = input.rows();
    std::shared_ptr<TensorData> output;

    switch (row_desc.type()) {
        case RowType::TimeFrameIndex:
            output = std::make_shared<TensorData>(
                    TensorData::createTimeSeries2D(
                            flat_data,
                            num_rows,
                            n_components,
                            row_desc.timeStoragePtr(),
                            row_desc.timeFrame(),
                            col_names));
            break;

        case RowType::Interval:
            output = std::make_shared<TensorData>(
                    TensorData::createFromIntervals(
                            flat_data,
                            num_rows,
                            n_components,
                            std::vector<TimeFrameInterval>(
                                    row_desc.intervals().begin(),
                                    row_desc.intervals().end()),
                            row_desc.timeFrame(),
                            col_names));
            break;

        case RowType::Ordinal:
            output = std::make_shared<TensorData>(
                    TensorData::createOrdinal2D(
                            flat_data,
                            num_rows,
                            n_components,
                            col_names));
            break;
    }

    ctx.reportProgress(100);
    return output;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
