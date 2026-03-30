/**
 * @file Tensor_Data_numpy.cpp
 * @brief Numpy (.npy) loading and saving implementation for TensorData objects
 */

#include "Tensor_Data_numpy.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TimeFrame/interval_data.hpp"
#include "npy.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

/**
 * @brief Derive the sidecar row-label path from the data file path
 *
 * For "tensor.npy" → "tensor_rows.npy"
 */
std::filesystem::path rowsSidecarPath(std::filesystem::path const & data_path) {
    auto stem = data_path.stem().string();
    auto parent = data_path.parent_path();
    return parent / (stem + "_rows.npy");
}

}// anonymous namespace

std::shared_ptr<TensorData> load(NpyTensorLoaderOptions const & options) {
    if (!std::filesystem::exists(options.filepath)) {
        throw std::runtime_error("Could not open .npy file: " + options.filepath);
    }

    auto npy_data = npy::read_npy<float>(options.filepath);

    // Convert shape to size_t vector
    std::vector<std::size_t> full_shape;
    full_shape.reserve(npy_data.shape.size());
    for (auto const dim: npy_data.shape) {
        full_shape.push_back(static_cast<std::size_t>(dim));
    }

    if (full_shape.empty()) {
        throw std::runtime_error("Empty tensor shape in .npy file: " + options.filepath);
    }

    // First dimension is rows; remaining dimensions are flattened into columns
    auto const num_rows = full_shape[0];
    std::size_t num_cols = 1;
    for (std::size_t i = 1; i < full_shape.size(); ++i) {
        num_cols *= full_shape[i];
    }

    // Check for a rows sidecar file
    auto const sidecar = rowsSidecarPath(options.filepath);
    if (std::filesystem::exists(sidecar)) {
        auto rows_npy = npy::read_npy<int64_t>(sidecar);

        if (rows_npy.shape.size() == 2 && rows_npy.shape[1] == 2) {
            // Interval: Nx2 array of [start, end]
            std::vector<TimeFrameInterval> intervals;
            intervals.reserve(num_rows);
            for (std::size_t r = 0; r < num_rows; ++r) {
                auto const start = rows_npy.data[r * 2];
                auto const end = rows_npy.data[r * 2 + 1];
                intervals.push_back({TimeFrameIndex{start}, TimeFrameIndex{end}});
            }
            auto tensor = TensorData::createFromIntervals(
                    npy_data.data, num_rows, num_cols,
                    std::move(intervals), nullptr, {});
            return std::make_shared<TensorData>(std::move(tensor));
        }

        // 1D array: TimeFrameIndex values
        if (rows_npy.shape.size() == 1) {
            // Check if it's just sequential 0..N-1 (ordinal)
            bool is_ordinal = true;
            for (std::size_t i = 0; i < num_rows && i < rows_npy.data.size(); ++i) {
                if (rows_npy.data[i] != static_cast<int64_t>(i)) {
                    is_ordinal = false;
                    break;
                }
            }

            if (!is_ordinal) {
                // Non-trivial time indices
                std::vector<TimeFrameIndex> time_indices;
                time_indices.reserve(num_rows);
                for (std::size_t i = 0; i < num_rows && i < rows_npy.data.size(); ++i) {
                    time_indices.emplace_back(rows_npy.data[i]);
                }
                auto time_storage = TimeIndexStorageFactory::createFromTimeIndices(time_indices);
                auto tensor = TensorData::createTimeSeries2D(
                        npy_data.data, num_rows, num_cols,
                        std::move(time_storage), nullptr, {});
                return std::make_shared<TensorData>(std::move(tensor));
            }
        }
    }

    // Default: ordinal rows
    auto tensor = TensorData::createOrdinal2D(
            npy_data.data,
            num_rows,
            num_cols,
            {});

    return std::make_shared<TensorData>(std::move(tensor));
}

bool save(TensorData const * tensor, NpyTensorSaverOptions const & opts) {
    assert(tensor && "save: tensor must not be null");

    auto const shape = tensor->shape();
    assert(shape.size() >= 2 && "save: tensor must be at least 2D");

    auto const target_path = std::filesystem::path(opts.parent_dir) / opts.filename;

    auto const num_rows = shape[0];
    auto const num_cols = shape[1];

    // Gather all data into a flat vector in row-major order
    std::vector<float> flat_data;
    flat_data.reserve(num_rows * num_cols);
    std::vector<std::size_t> indices(shape.size(), 0);
    for (std::size_t r = 0; r < num_rows; ++r) {
        indices[0] = r;
        for (std::size_t c = 0; c < num_cols; ++c) {
            indices[1] = c;
            flat_data.push_back(tensor->at(std::span<std::size_t const>{indices}));
        }
    }

    npy::npy_data<float> npy_out;
    npy_out.data = std::move(flat_data);
    npy_out.shape = {static_cast<unsigned long>(num_rows),
                     static_cast<unsigned long>(num_cols)};
    npy_out.fortran_order = false;

    try {
        npy::write_npy(target_path.string(), npy_out);
    } catch (std::exception const & e) {
        std::cerr << "Failed to save tensor to " << target_path << ": " << e.what() << std::endl;
        return false;
    }

    // Write rows sidecar
    auto const sidecar = rowsSidecarPath(target_path);
    auto const & rows = tensor->rows();

    try {
        switch (rows.type()) {
            case RowType::Ordinal: {
                // 1D int64 array: [0, 1, ..., N-1]
                std::vector<int64_t> row_labels(num_rows);
                for (std::size_t i = 0; i < num_rows; ++i) {
                    row_labels[i] = static_cast<int64_t>(i);
                }
                npy::npy_data<int64_t> rows_out;
                rows_out.data = std::move(row_labels);
                rows_out.shape = {static_cast<unsigned long>(num_rows)};
                rows_out.fortran_order = false;
                npy::write_npy(sidecar.string(), rows_out);
                break;
            }
            case RowType::TimeFrameIndex: {
                // 1D int64 array of TimeFrameIndex values
                auto const & storage = rows.timeStorage();
                std::vector<int64_t> row_labels(num_rows);
                for (std::size_t i = 0; i < num_rows; ++i) {
                    row_labels[i] = storage.getTimeFrameIndexAt(i).getValue();
                }
                npy::npy_data<int64_t> rows_out;
                rows_out.data = std::move(row_labels);
                rows_out.shape = {static_cast<unsigned long>(num_rows)};
                rows_out.fortran_order = false;
                npy::write_npy(sidecar.string(), rows_out);
                break;
            }
            case RowType::Interval: {
                // 2D int64 array (N, 2) with [start, end] per row
                auto intervals = rows.intervals();
                std::vector<int64_t> row_labels(num_rows * 2);
                for (std::size_t i = 0; i < num_rows; ++i) {
                    row_labels[i * 2] = intervals[i].start.getValue();
                    row_labels[i * 2 + 1] = intervals[i].end.getValue();
                }
                npy::npy_data<int64_t> rows_out;
                rows_out.data = std::move(row_labels);
                rows_out.shape = {static_cast<unsigned long>(num_rows), 2UL};
                rows_out.fortran_order = false;
                npy::write_npy(sidecar.string(), rows_out);
                break;
            }
        }
    } catch (std::exception const & e) {
        std::cerr << "Failed to save rows sidecar to " << sidecar << ": " << e.what() << std::endl;
        // Data file was written successfully; sidecar failure is non-fatal
    }

    std::cout << "Successfully saved tensor data to " << target_path << std::endl;
    return true;
}
