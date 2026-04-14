/**
 * @file TensorTemporalNeighbors.cpp
 * @brief Implementation of temporal neighbor feature augmentation for TensorData
 */

#include "TensorTemporalNeighbors.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "core/ComputeContext.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// buildOffsets
// ============================================================================

std::vector<int> TensorTemporalNeighborParams::buildOffsets() const {
    std::vector<int> result;

    // Lags (negative, descending magnitude → ascending value: -range, ..., -step)
    if (lag_range > 0 && lag_step >= 1) {
        for (int v = lag_range; v >= lag_step; v -= lag_step) {
            result.push_back(-v);
        }
    }

    // Leads (positive, ascending)
    if (lead_range > 0 && lead_step >= 1) {
        for (int v = lead_step; v <= lead_range; v += lead_step) {
            result.push_back(v);
        }
    }

    return result;
}

namespace {

/// Build column names for one offset group (all columns)
std::vector<std::string> buildOffsetColumnNames(
        std::vector<std::string> const & src_names,
        std::size_t num_cols,
        int offset) {
    std::vector<std::string> names;
    names.reserve(num_cols);
    for (std::size_t ci = 0; ci < num_cols; ++ci) {
        std::string const & base =
                ci < src_names.size() ? src_names[ci] : ("col" + std::to_string(ci));
        names.push_back(base + "_lag" + (offset >= 0 ? "+" : "") + std::to_string(offset));
    }
    return names;
}

/// Get fill value for a given boundary policy
float fillValue(BoundaryPolicy policy) {
    switch (policy) {
        case BoundaryPolicy::NaN:
            return std::numeric_limits<float>::quiet_NaN();
        case BoundaryPolicy::Zero:
        case BoundaryPolicy::Clamp:
        case BoundaryPolicy::Drop:
        default:
            return 0.0f;
    }
}

}// namespace

auto tensorTemporalNeighbors(
        TensorData const & input,
        TensorTemporalNeighborParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData> {

    // --- Validation ---
    if (input.ndim() != 2) {
        ctx.logMessage("TensorTemporalNeighbors: Input must be 2D, got " +
                       std::to_string(input.ndim()) + "D");
        return nullptr;
    }

    if (input.rowType() != RowType::TimeFrameIndex) {
        ctx.logMessage("TensorTemporalNeighbors: Input must have TimeFrameIndex rows");
        return nullptr;
    }

    std::size_t const num_rows = input.numRows();
    std::size_t const num_cols = input.numColumns();

    if (num_rows == 0) {
        ctx.logMessage("TensorTemporalNeighbors: Input has no rows");
        return nullptr;
    }

    // Build offsets from lag/lead range parameters
    auto const offsets = params.buildOffsets();

    // If no offsets, return a copy of the input (identity transform)
    if (offsets.empty()) {
        auto const & row_desc = input.rows();
        auto data = input.materializeFlat();
        return std::make_shared<TensorData>(
                TensorData::createTimeSeries2D(
                        data, num_rows, num_cols,
                        row_desc.timeStoragePtr(),
                        row_desc.timeFrame(),
                        input.columnNames()));
    }

    ctx.reportProgress(0);

    // --- Determine surviving rows (Drop mode) ---
    std::vector<std::size_t> surviving_rows;
    if (params.boundary_policy == BoundaryPolicy::Drop) {
        for (std::size_t r = 0; r < num_rows; ++r) {
            bool all_valid = true;
            for (int const offset: offsets) {
                auto const neighbor = static_cast<std::ptrdiff_t>(r) + offset;
                if (neighbor < 0 || neighbor >= static_cast<std::ptrdiff_t>(num_rows)) {
                    all_valid = false;
                    break;
                }
            }
            if (all_valid) {
                surviving_rows.push_back(r);
            }
        }
        if (surviving_rows.empty()) {
            ctx.logMessage("TensorTemporalNeighbors: Drop policy eliminated all rows");
            return nullptr;
        }
    }

    if (ctx.shouldCancel()) return nullptr;
    ctx.reportProgress(10);

    // --- Compute output dimensions ---
    std::size_t const num_offsets = offsets.size();
    std::size_t const original_cols_in_output = params.include_original ? num_cols : 0;
    std::size_t const total_out_cols =
            original_cols_in_output + num_cols * num_offsets;
    std::size_t const out_rows =
            (params.boundary_policy == BoundaryPolicy::Drop) ? surviving_rows.size() : num_rows;

    // --- Build column names ---
    std::vector<std::string> out_col_names;
    out_col_names.reserve(total_out_cols);

    if (params.include_original) {
        auto const & src_names = input.columnNames();
        for (std::size_t c = 0; c < num_cols; ++c) {
            out_col_names.push_back(
                    c < src_names.size() ? src_names[c] : ("col" + std::to_string(c)));
        }
    }

    for (int const offset: offsets) {
        auto names = buildOffsetColumnNames(input.columnNames(), num_cols, offset);
        out_col_names.insert(out_col_names.end(), names.begin(), names.end());
    }

    // --- Build flat output data (row-major) ---
    auto const src_flat = input.materializeFlat();
    std::vector<float> out_data(out_rows * total_out_cols);
    float const fill = fillValue(params.boundary_policy);

    for (std::size_t out_r = 0; out_r < out_rows; ++out_r) {
        std::size_t const src_r =
                (params.boundary_policy == BoundaryPolicy::Drop) ? surviving_rows[out_r] : out_r;

        std::size_t out_col = 0;

        // Original columns
        if (params.include_original) {
            for (std::size_t c = 0; c < num_cols; ++c) {
                out_data[out_r * total_out_cols + out_col] = src_flat[src_r * num_cols + c];
                ++out_col;
            }
        }

        // Shifted columns for each offset (all columns)
        for (int const offset: offsets) {
            auto const neighbor = static_cast<std::ptrdiff_t>(src_r) + offset;
            bool const in_bounds =
                    (neighbor >= 0) && (neighbor < static_cast<std::ptrdiff_t>(num_rows));

            std::size_t source_row = 0;
            if (in_bounds) {
                source_row = static_cast<std::size_t>(neighbor);
            } else if (params.boundary_policy == BoundaryPolicy::Clamp) {
                source_row = (neighbor < 0) ? 0 : (num_rows - 1);
            }

            for (std::size_t ci = 0; ci < num_cols; ++ci) {
                if (in_bounds || params.boundary_policy == BoundaryPolicy::Clamp) {
                    out_data[out_r * total_out_cols + out_col] =
                            src_flat[source_row * num_cols + ci];
                } else {
                    out_data[out_r * total_out_cols + out_col] = fill;
                }
                ++out_col;
            }
        }

        if (ctx.shouldCancel()) return nullptr;
    }

    ctx.reportProgress(80);

    // --- Reconstruct TensorData preserving TimeFrame ---
    auto const & row_desc = input.rows();

    std::shared_ptr<TimeIndexStorage> out_time_storage;
    if (params.boundary_policy == BoundaryPolicy::Drop) {
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
