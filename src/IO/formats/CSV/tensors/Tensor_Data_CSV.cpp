/**
 * @file Tensor_Data_CSV.cpp
 * @brief CSV loading and saving implementation for TensorData objects
 */

#include "Tensor_Data_CSV.hpp"

#include "IO/core/AtomicWrite.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

/**
 * @brief Detect the row type from the first data cell
 *
 * - Contains '-' and both parts are integers → Interval
 * - Plain integer → TimeFrameIndex
 * - Otherwise → Ordinal
 */
RowType detectRowType(std::string const & first_cell) {
    if (first_cell.find('-') != std::string::npos) {
        auto const pos = first_cell.find('-');
        // Check it's not just a negative number (dash at position 0)
        if (pos > 0) {
            return RowType::Interval;
        }
    }

    // Try parsing as integer → TimeFrameIndex
    try {
        std::stoll(first_cell);
        return RowType::TimeFrameIndex;
    } catch (...) {
        return RowType::Ordinal;
    }
}

/**
 * @brief Parse a row label string into a TimeFrameInterval
 * @pre cell contains a '-' at a position > 0 (i.e., "start-end")
 */
TimeFrameInterval parseInterval(std::string const & cell) {
    auto const pos = cell.find('-');
    assert(pos != std::string::npos && pos > 0 && "parseInterval: expected 'start-end' format");
    auto const start = std::stoll(cell.substr(0, pos));
    auto const end = std::stoll(cell.substr(pos + 1));
    return TimeFrameInterval{TimeFrameIndex{start}, TimeFrameIndex{end}};
}

}// anonymous namespace

std::shared_ptr<TensorData> load(CSVTensorLoaderOptions const & options) {
    std::ifstream file(options.filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open CSV file: " + options.filepath);
    }

    char const delim = options.delimiter.empty() ? ',' : options.delimiter[0];

    // Read header
    std::vector<std::string> column_names;
    std::string line;
    if (options.has_header && std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;
        bool first = true;
        while (std::getline(ss, cell, delim)) {
            if (first) {
                first = false;// skip the row label column header
                continue;
            }
            // Trim trailing \r
            if (!cell.empty() && cell.back() == '\r') {
                cell.pop_back();
            }
            column_names.push_back(cell);
        }
    }

    // Read data rows
    std::vector<std::string> row_labels;
    std::vector<float> flat_data;
    std::size_t num_cols = 0;

    while (std::getline(file, line)) {
        if (line.empty() || (line.size() == 1 && line[0] == '\r')) {
            continue;
        }

        std::stringstream ss(line);
        std::string cell;
        bool first = true;
        std::size_t col_count = 0;

        while (std::getline(ss, cell, delim)) {
            // Trim trailing \r
            if (!cell.empty() && cell.back() == '\r') {
                cell.pop_back();
            }

            if (first) {
                row_labels.push_back(cell);
                first = false;
                continue;
            }

            flat_data.push_back(std::stof(cell));
            ++col_count;
        }

        if (num_cols == 0) {
            num_cols = col_count;
        }
    }

    if (row_labels.empty() || num_cols == 0) {
        throw std::runtime_error("No valid data found in CSV file: " + options.filepath);
    }

    auto const num_rows = row_labels.size();

    // Detect row type from first label
    auto const detected_type = detectRowType(row_labels[0]);

    switch (detected_type) {
        case RowType::TimeFrameIndex: {
            std::vector<TimeFrameIndex> time_indices;
            time_indices.reserve(num_rows);
            for (auto const & label: row_labels) {
                time_indices.emplace_back(std::stoll(label));
            }

            auto time_storage = TimeIndexStorageFactory::createFromTimeIndices(time_indices);

            auto tensor = TensorData::createTimeSeries2D(
                    flat_data, num_rows, num_cols,
                    std::move(time_storage), nullptr,
                    column_names);
            return std::make_shared<TensorData>(std::move(tensor));
        }
        case RowType::Interval: {
            std::vector<TimeFrameInterval> intervals;
            intervals.reserve(num_rows);
            for (auto const & label: row_labels) {
                auto interval = parseInterval(label);
                intervals.push_back(interval);
            }

            auto tensor = TensorData::createFromIntervals(
                    flat_data, num_rows, num_cols,
                    std::move(intervals), nullptr,
                    column_names);
            return std::make_shared<TensorData>(std::move(tensor));
        }
        case RowType::Ordinal: {
            auto tensor = TensorData::createOrdinal2D(
                    flat_data, num_rows, num_cols, column_names);
            return std::make_shared<TensorData>(std::move(tensor));
        }
    }

    // Unreachable, but satisfy compiler
    throw std::runtime_error("Unknown row type detected in CSV file: " + options.filepath);
}

bool save(TensorData const * tensor, CSVTensorSaverOptions const & opts) {
    assert(tensor && "save: tensor must not be null");

    auto const shape = tensor->shape();
    assert(shape.size() >= 2 && "save: tensor must be at least 2D");

    auto const target_path = std::filesystem::path(opts.parent_dir) / opts.filename;

    auto const num_rows = shape[0];
    auto const num_cols = shape[1];
    auto const & dims = tensor->dimensions();
    auto const & rows = tensor->rows();

    char const delim = opts.delimiter.empty() ? ',' : opts.delimiter[0];

    bool const ok = atomicWriteFile(target_path, [&](std::ostream & out) {
        // Write header
        if (opts.save_header) {
            out << "row";
            if (dims.hasColumnNames()) {
                for (auto const & cn: dims.columnNames()) {
                    out << delim << cn;
                }
            } else {
                for (std::size_t c = 0; c < num_cols; ++c) {
                    out << delim << "col_" << c;
                }
            }
            out << "\n";
        }

        // Write data
        out << std::fixed;
        out.precision(opts.precision);

        for (std::size_t r = 0; r < num_rows; ++r) {
            // Row label
            auto label = rows.labelAt(r);
            if (auto const * ordinal = std::get_if<std::size_t>(&label)) {
                out << *ordinal;
            } else if (auto const * tfi = std::get_if<TimeFrameIndex>(&label)) {
                out << tfi->getValue();
            } else if (auto const * interval = std::get_if<TimeFrameInterval>(&label)) {
                out << interval->start.getValue() << "-" << interval->end.getValue();
            } else {
                out << r;
            }

            // Column values
            std::vector<std::size_t> indices(shape.size(), 0);
            indices[0] = r;
            for (std::size_t c = 0; c < num_cols; ++c) {
                indices[1] = c;
                float const val = tensor->at(std::span<std::size_t const>{indices});
                out << delim << val;
            }
            out << "\n";
        }

        return out.good();
    });

    if (ok) {
        std::cout << "Successfully saved tensor data to " << target_path << std::endl;
    }
    return ok;
}
