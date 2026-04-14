/**
 * @file DimReductionOutputBuilder.cpp
 * @brief Implementation of shared output tensor construction for dim reduction
 *
 * @see MLCore::DimReductionPipeline (DimReductionPipeline.cpp) for the
 *      equivalent buildOutputTensor() / filterRowTimes() used by the
 *      MLCore_Widget pathway.
 */

#include "DimReductionOutputBuilder.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <cmath>
#include <limits>

namespace WhiskerToolbox::Transforms::V2 {

namespace {

/// Convert reduced matrix (n_components × observations, mlpack layout) to row-major flat float data
std::vector<float> toRowMajorFlat(arma::mat const & reduced) {
    arma::fmat const output_fmat = arma::conv_to<arma::fmat>::from(reduced.t());
    auto const num_rows = output_fmat.n_rows;
    auto const num_cols = output_fmat.n_cols;

    std::vector<float> flat(output_fmat.n_elem);
    for (arma::uword r = 0; r < num_rows; ++r) {
        for (arma::uword c = 0; c < num_cols; ++c) {
            flat[r * num_cols + c] = output_fmat(r, c);
        }
    }
    return flat;
}

/// Build full-size flat data with NaN-filled rows for positions not in valid_indices
std::vector<float> buildPropagateFlat(
        arma::mat const & reduced,
        std::size_t total_rows,
        std::size_t n_components,
        std::vector<std::size_t> const & valid_indices) {

    float const nan_val = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> flat(total_rows * n_components, nan_val);

    arma::fmat const output_fmat = arma::conv_to<arma::fmat>::from(reduced.t());

    for (std::size_t vi = 0; vi < valid_indices.size(); ++vi) {
        std::size_t const orig_row = valid_indices[vi];
        for (std::size_t c = 0; c < n_components; ++c) {
            flat[orig_row * n_components + c] = output_fmat(
                    static_cast<arma::uword>(vi),
                    static_cast<arma::uword>(c));
        }
    }
    return flat;
}

/// Filter TimeFrameIndex values to only those at valid_indices
std::vector<TimeFrameIndex> filterRowTimes(
        RowDescriptor const & row_desc,
        std::vector<std::size_t> const & valid_indices) {
    auto const & ts = row_desc.timeStorage();
    std::vector<TimeFrameIndex> filtered;
    filtered.reserve(valid_indices.size());
    for (auto idx: valid_indices) {
        filtered.push_back(ts.getTimeFrameIndexAt(idx));
    }
    return filtered;
}

}// namespace

std::shared_ptr<TensorData> buildDimReductionOutput(
        arma::mat const & reduced,
        TensorData const & input,
        std::vector<std::size_t> const & valid_indices,
        NaNPolicy policy,
        std::size_t n_components,
        std::vector<std::string> const & col_names) {

    auto const & row_desc = input.rows();
    std::size_t const total_rows = input.numRows();
    bool const is_propagate = (policy == NaNPolicy::Propagate);

    // Determine output dimensions and flat data
    std::size_t out_rows = 0;
    std::vector<float> flat_data;

    if (is_propagate) {
        out_rows = total_rows;
        flat_data = buildPropagateFlat(reduced, total_rows, n_components, valid_indices);
    } else {
        // Drop mode
        out_rows = valid_indices.size();
        flat_data = toRowMajorFlat(reduced);
    }

    // Build output TensorData preserving RowDescriptor
    switch (row_desc.type()) {
        case RowType::TimeFrameIndex: {
            if (is_propagate) {
                // Keep original time storage — same row count
                return std::make_shared<TensorData>(
                        TensorData::createTimeSeries2D(
                                flat_data,
                                out_rows,
                                n_components,
                                row_desc.timeStoragePtr(),
                                row_desc.timeFrame(),
                                col_names));
            }
            // Drop: build filtered time storage
            auto filtered_times = filterRowTimes(row_desc, valid_indices);
            auto time_storage = TimeIndexStorageFactory::createFromTimeIndices(
                    std::move(filtered_times));
            return std::make_shared<TensorData>(
                    TensorData::createTimeSeries2D(
                            flat_data,
                            out_rows,
                            n_components,
                            time_storage,
                            row_desc.timeFrame(),
                            col_names));
        }

        case RowType::Interval: {
            if (is_propagate) {
                // Keep original intervals — same row count
                return std::make_shared<TensorData>(
                        TensorData::createFromIntervals(
                                flat_data,
                                out_rows,
                                n_components,
                                std::vector<TimeFrameInterval>(
                                        row_desc.intervals().begin(),
                                        row_desc.intervals().end()),
                                row_desc.timeFrame(),
                                col_names));
            }
            // Drop: filter intervals
            auto const & all_intervals = row_desc.intervals();
            std::vector<TimeFrameInterval> filtered_intervals;
            filtered_intervals.reserve(valid_indices.size());
            for (auto idx: valid_indices) {
                filtered_intervals.push_back(all_intervals[idx]);
            }
            return std::make_shared<TensorData>(
                    TensorData::createFromIntervals(
                            flat_data,
                            out_rows,
                            n_components,
                            std::move(filtered_intervals),
                            row_desc.timeFrame(),
                            col_names));
        }

        case RowType::Ordinal:
            return std::make_shared<TensorData>(
                    TensorData::createOrdinal2D(
                            flat_data,
                            out_rows,
                            n_components,
                            col_names));
    }

    return nullptr;
}

}// namespace WhiskerToolbox::Transforms::V2
