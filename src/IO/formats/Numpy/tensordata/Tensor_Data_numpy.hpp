/**
 * @file Tensor_Data_numpy.hpp
 * @brief Numpy (.npy) loading and saving for TensorData objects
 *
 * Supports 2D tensors stored as .npy files. The first dimension is treated as
 * the row axis; remaining dimensions are flattened into features/columns.
 *
 * A companion sidecar file `<stem>_rows.npy` stores row labels:
 * - Ordinal / TimeFrameIndex rows: 1D int64 array of row labels
 * - Interval rows: 2D int64 array (N, 2) with [start, end] per row
 *
 * On load, if the sidecar is absent the tensor defaults to ordinal rows.
 */

#ifndef WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP
#define WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP

#include "datamanagerio_numpy_export.h"

#include "IO/core/LoaderOptionsConcepts.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <memory>
#include <string>

class TensorData;

/**
 * @brief Options for loading TensorData from a .npy file
 *
 * The .npy file is expected to contain a numeric array whose first dimension
 * represents rows. Any additional dimensions are flattened into a single
 * feature/column axis.
 *
 * The row_type field controls how rows are interpreted:
 * - "ordinal": rows are plain indices 0..N-1 (default)
 * - "time":    rows correspond to dense sequential TimeFrameIndex values [0, N)
 * - "interval": rows correspond to time intervals (pairs of start/end indices)
 *
 * Note: the TimeFrame itself is assigned by DataManager when the data is
 * registered via setData(). The loader only tags the intended row semantics.
 */
struct NpyTensorLoaderOptions {
    std::string filepath;
    std::string row_type = "ordinal";
};

static_assert(WhiskerToolbox::ValidLoaderOptions<NpyTensorLoaderOptions>,
              "NpyTensorLoaderOptions must satisfy ValidLoaderOptions");

/**
 * @brief Options for saving TensorData to a .npy file
 *
 * The tensor data is written as a 2D float32 array in C (row-major) order.
 * A companion sidecar `<stem>_rows.npy` is written alongside the data file
 * containing the row labels (ordinal indices, time indices, or interval pairs).
 */
struct NpyTensorSaverOptions {
    std::string filename;
    std::string parent_dir = ".";
};

/**
 * @brief Load a TensorData from a .npy file
 * @param options Loader options including filepath
 * @return Shared pointer to the loaded TensorData
 * @throws std::runtime_error on file or parse errors
 */
DATAMANAGERIO_NUMPY_EXPORT std::shared_ptr<TensorData> load(NpyTensorLoaderOptions const & options);

/**
 * @brief Save a TensorData to a .npy file
 * @pre tensor must not be null
 * @pre tensor must be at least 2D
 * @param tensor Pointer to the TensorData to save (non-owning, caller retains ownership)
 * @param opts Saver options including filename and parent_dir
 * @return true on success
 */
DATAMANAGERIO_NUMPY_EXPORT bool save(TensorData const * tensor, NpyTensorSaverOptions const & opts);

template<>
struct ParameterUIHints<NpyTensorLoaderOptions> {
    /// @brief Annotate schema fields for AutoParamWidget (import UI).
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filepath")) {
            f->tooltip = "Path to .npy file containing a numeric array";
        }
        if (auto * f = schema.field("row_type")) {
            f->tooltip = "How to interpret rows: ordinal (plain indices), time (TimeFrameIndex), or interval";
            f->allowed_values = {"ordinal", "time", "interval"};
        }
    }
};

template<>
struct ParameterUIHints<NpyTensorSaverOptions> {
    /// @brief Annotate schema fields for AutoParamWidget (export UI).
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filename")) {
            f->tooltip = "Output .npy filename (combined with parent_dir)";
        }
        if (auto * f = schema.field("parent_dir")) {
            f->tooltip = "Directory in which to create the output file";
        }
    }
};

#endif// WHISKERTOOLBOX_TENSOR_DATA_NUMPY_HPP
