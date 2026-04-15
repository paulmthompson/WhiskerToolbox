/**
 * @file TensorCentralDifference.cpp
 * @brief Implementation of central-difference feature augmentation for TensorData
 */

#include "TensorCentralDifference.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "core/ComputeContext.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

namespace {

/// Get fill value for a given boundary policy
float fillValue(DeltaBoundaryPolicy policy) {
    switch (policy) {
        case DeltaBoundaryPolicy::NaN:
            return std::numeric_limits<float>::quiet_NaN();
        case DeltaBoundaryPolicy::Zero:
        case DeltaBoundaryPolicy::Clamp:
        case DeltaBoundaryPolicy::Drop:
        default:
            return 0.0f;
    }
}

}// namespace

auto tensorCentralDifference(
        TensorData const & input,
        TensorCentralDifferenceParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData> {

    // --- Validation ---
    if (input.ndim() != 2) {
        ctx.logMessage("TensorCentralDifference: Input must be 2D, got " +
                       std::to_string(input.ndim()) + "D");
        return nullptr;
    }

    if (input.rowType() != RowType::TimeFrameIndex) {
        ctx.logMessage("TensorCentralDifference: Input must have TimeFrameIndex rows");
        return nullptr;
    }

    std::size_t const num_rows = input.numRows();
    std::size_t const num_cols = input.numColumns();

    if (num_rows == 0) {
        ctx.logMessage("TensorCentralDifference: Input has no rows");
        return nullptr;
    }

    ctx.reportProgress(0);

    // --- Determine surviving rows (Drop mode) ---
    std::vector<std::size_t> surviving_rows;
    if (params.boundary_policy == DeltaBoundaryPolicy::Drop) {
        // First and last rows are boundary rows
        for (std::size_t r = 1; r + 1 < num_rows; ++r) {
            surviving_rows.push_back(r);
        }
        if (surviving_rows.empty()) {
            ctx.logMessage("TensorCentralDifference: Drop policy eliminated all rows");
            return nullptr;
        }
    }

    if (ctx.shouldCancel()) return nullptr;
    ctx.reportProgress(10);

    // --- Compute output dimensions ---
    std::size_t const original_cols_in_output = params.include_original ? num_cols : 0;
    std::size_t const total_out_cols = original_cols_in_output + num_cols;
    std::size_t const out_rows =
            (params.boundary_policy == DeltaBoundaryPolicy::Drop) ? surviving_rows.size() : num_rows;

    // --- Build column names ---
    std::vector<std::string> out_col_names;
    out_col_names.reserve(total_out_cols);

    auto const & src_names = input.columnNames();
    if (params.include_original) {
        for (std::size_t c = 0; c < num_cols; ++c) {
            out_col_names.push_back(
                    c < src_names.size() ? src_names[c] : ("col" + std::to_string(c)));
        }
    }

    for (std::size_t c = 0; c < num_cols; ++c) {
        std::string const & base =
                c < src_names.size() ? src_names[c] : ("col" + std::to_string(c));
        out_col_names.push_back(base + "_delta");
    }

    // --- Build flat output data (row-major) ---
    auto const src_flat = input.materializeFlat();
    std::vector<float> out_data(out_rows * total_out_cols);
    float const fill = fillValue(params.boundary_policy);

    for (std::size_t out_r = 0; out_r < out_rows; ++out_r) {
        std::size_t const src_r =
                (params.boundary_policy == DeltaBoundaryPolicy::Drop) ? surviving_rows[out_r] : out_r;

        std::size_t out_col = 0;

        // Original columns
        if (params.include_original) {
            for (std::size_t c = 0; c < num_cols; ++c) {
                out_data[out_r * total_out_cols + out_col] = src_flat[src_r * num_cols + c];
                ++out_col;
            }
        }

        // Central difference columns
        bool const has_prev = (src_r > 0);
        bool const has_next = (src_r + 1 < num_rows);

        for (std::size_t c = 0; c < num_cols; ++c) {
            if (has_prev && has_next) {
                // Standard central difference: (f(t+1) - f(t-1)) / 2
                float const next_val = src_flat[(src_r + 1) * num_cols + c];
                float const prev_val = src_flat[(src_r - 1) * num_cols + c];
                out_data[out_r * total_out_cols + out_col] = (next_val - prev_val) / 2.0f;
            } else if (params.boundary_policy == DeltaBoundaryPolicy::Clamp) {
                // Clamp: use current row value for the missing neighbor
                std::size_t const prev_row = has_prev ? (src_r - 1) : src_r;
                std::size_t const next_row = has_next ? (src_r + 1) : src_r;
                float const next_val = src_flat[next_row * num_cols + c];
                float const prev_val = src_flat[prev_row * num_cols + c];
                out_data[out_r * total_out_cols + out_col] = (next_val - prev_val) / 2.0f;
            } else {
                // NaN or Zero fill
                out_data[out_r * total_out_cols + out_col] = fill;
            }
            ++out_col;
        }

        if (ctx.shouldCancel()) return nullptr;
    }

    ctx.reportProgress(80);

    // --- Reconstruct TensorData preserving TimeFrame ---
    auto const & row_desc = input.rows();

    std::shared_ptr<TimeIndexStorage> out_time_storage;
    if (params.boundary_policy == DeltaBoundaryPolicy::Drop) {
        // Build new SparseTimeIndexStorage from surviving row indices
        auto const & orig_storage = row_desc.timeStorage();
        std::vector<TimeFrameIndex> surviving_times;
        surviving_times.reserve(surviving_rows.size());
        for (auto r: surviving_rows) {
            surviving_times.push_back(orig_storage.getTimeFrameIndexAt(r));
        }
        out_time_storage = std::make_shared<SparseTimeIndexStorage>(std::move(surviving_times));
    } else {
        out_time_storage = row_desc.timeStoragePtr();
    }

    ctx.reportProgress(100);

    return std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(
                    out_data,
                    out_rows,
                    total_out_cols,
                    out_time_storage,
                    row_desc.timeFrame(),
                    out_col_names));
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
