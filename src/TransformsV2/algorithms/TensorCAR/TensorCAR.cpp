/**
 * @file TensorCAR.cpp
 * @brief Implementation of Common Average Reference container transform
 */

#include "TensorCAR.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <armadillo>

#include <cassert>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

auto tensorCAR(
        TensorData const & input,
        TensorCARParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData> {

    // --- Validation ---
    if (input.ndim() != 2) {
        ctx.logMessage("TensorCAR: Input must be 2D, got " +
                       std::to_string(input.ndim()) + "D");
        return nullptr;
    }

    std::size_t const num_rows = input.numRows();
    std::size_t const num_cols = input.numColumns();

    if (num_rows == 0) {
        ctx.logMessage("TensorCAR: Input has no rows");
        return nullptr;
    }

    if (num_cols == 0) {
        ctx.logMessage("TensorCAR: Input has no channels");
        return nullptr;
    }

    if (params.use_gpu) {
        ctx.logMessage("TensorCAR: use_gpu=true is not yet implemented; "
                       "falling back to CPU (Armadillo).");
    }

    ctx.reportProgress(0);

    // --- Build included-column index set ---
    // Excluded channels are not used when computing the reference, but
    // the reference is still subtracted from them.
    std::set<int> const excluded_set(
            params.exclude_channels.begin(),
            params.exclude_channels.end());

    std::vector<arma::uword> included_col_indices;
    included_col_indices.reserve(num_cols);
    for (std::size_t c = 0; c < num_cols; ++c) {
        if (excluded_set.find(static_cast<int>(c)) == excluded_set.end()) {
            included_col_indices.push_back(static_cast<arma::uword>(c));
        }
    }

    if (included_col_indices.empty()) {
        ctx.logMessage("TensorCAR: All channels are excluded — no reference to compute");
        return nullptr;
    }

    // --- Retrieve Armadillo matrix ---
    arma::fmat const & input_mat = input.asArmadilloMatrix();
    // input_mat shape: (num_rows, num_cols) — column-major

    ctx.reportProgress(10);

    // --- Compute row-wise reference signal ---
    // ref[t] = mean or median of included channels at time point t
    arma::fmat ref_col;// shape: (num_rows, 1)

    bool const all_included = (included_col_indices.size() == num_cols);

    if (all_included) {
        // Operate on the full matrix — no sub-matrix needed
        if (params.method == CARMethod::Mean) {
            ref_col = arma::mean(input_mat, 1);
        } else {
            ref_col = arma::median(input_mat, 1);
        }
    } else {
        arma::uvec const include_uvec(included_col_indices);
        arma::fmat const ref_source = input_mat.cols(include_uvec);
        if (params.method == CARMethod::Mean) {
            ref_col = arma::mean(ref_source, 1);
        } else {
            ref_col = arma::median(ref_source, 1);
        }
    }

    assert(ref_col.n_rows == num_rows && "Reference vector length must equal num_rows");

    ctx.reportProgress(50);

    if (ctx.shouldCancel()) {
        return nullptr;
    }

    // --- Subtract reference from all channels ---
    // result.each_col() -= ref_col  →  result[t, c] -= ref_col[t]  for all t, c
    arma::fmat result_mat = input_mat;
    result_mat.each_col() -= ref_col;

    ctx.reportProgress(80);

    if (ctx.shouldCancel()) {
        return nullptr;
    }

    // --- Preserve column names ---
    auto const & src_col_names = input.columnNames();
    std::vector<std::string> const col_names(src_col_names.begin(), src_col_names.end());

    // --- Reconstruct TensorData preserving RowDescriptor ---
    auto const & row_desc = input.rows();

    ctx.reportProgress(100);

    switch (row_desc.type()) {
        case RowType::TimeFrameIndex: {
            return std::make_shared<TensorData>(
                    TensorData::createTimeSeries2D(
                            std::move(result_mat),
                            row_desc.timeStoragePtr(),
                            row_desc.timeFrame(),
                            col_names));
        }
        case RowType::Interval: {
            auto const intervals_span = row_desc.intervals();
            std::vector<TimeFrameInterval> intervals(
                    intervals_span.begin(),
                    intervals_span.end());
            return std::make_shared<TensorData>(
                    TensorData::createFromIntervals(
                            std::move(result_mat),
                            std::move(intervals),
                            row_desc.timeFrame(),
                            col_names));
        }
        case RowType::Ordinal:
        default: {
            return std::make_shared<TensorData>(
                    TensorData::createFromArmadillo(
                            std::move(result_mat),
                            col_names));
        }
    }
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
