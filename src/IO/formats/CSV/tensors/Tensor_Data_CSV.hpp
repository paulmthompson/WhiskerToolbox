/**
 * @file Tensor_Data_CSV.hpp
 * @brief CSV loading and saving for TensorData objects
 *
 * Supports 2D tensors with three row types: TimeFrameIndex, Interval, and Ordinal.
 * The CSV format uses a header row with column names and one data row per tensor row.
 *
 * Row label column format:
 * - TimeFrameIndex: integer time index
 * - Interval: "start-end" (e.g. "100-200")
 * - Ordinal: row index integer
 */

#ifndef TENSOR_DATA_CSV_HPP
#define TENSOR_DATA_CSV_HPP

#include "datamanagerio_export.h"

#include "IO/core/LoaderOptionsConcepts.hpp"

#include <memory>
#include <string>

class TensorData;

/**
 * @brief Options for loading TensorData from CSV
 *
 * The CSV file is expected to have:
 * - A header row with column names (first column is the row label)
 * - One data row per tensor row, with the row label in the first column
 *
 * Row types are auto-detected from the first data row:
 * - If the first cell contains a '-' character, it is parsed as an interval
 * - If it is a plain integer, it is parsed as a TimeFrameIndex
 * - Otherwise it is treated as ordinal
 */
struct CSVTensorLoaderOptions {
    std::string filepath;
    std::string delimiter = ",";
    bool has_header = true;
};

static_assert(WhiskerToolbox::ValidLoaderOptions<CSVTensorLoaderOptions>,
              "CSVTensorLoaderOptions must satisfy ValidLoaderOptions");

/**
 * @brief Options for saving TensorData to CSV
 */
struct CSVTensorSaverOptions {
    std::string filename;
    std::string parent_dir = ".";
    std::string delimiter = ",";
    bool save_header = true;
    int precision = 6;
};

/**
 * @brief Load a TensorData from a CSV file
 * @param options Loader options including filepath and delimiter
 * @return Shared pointer to the loaded TensorData
 * @throws std::runtime_error on file or parse errors
 */
DATAMANAGERIO_EXPORT std::shared_ptr<TensorData> load(CSVTensorLoaderOptions const & options);

/**
 * @brief Save a TensorData to a CSV file
 * @pre tensor must not be null
 * @pre tensor must be at least 2D
 * @param tensor Pointer to the TensorData to save (non-owning, caller retains ownership)
 * @param opts Saver options including filename, parent_dir, delimiter
 * @return true on success
 */
DATAMANAGERIO_EXPORT bool save(TensorData const * tensor, CSVTensorSaverOptions const & opts);

#endif// TENSOR_DATA_CSV_HPP
